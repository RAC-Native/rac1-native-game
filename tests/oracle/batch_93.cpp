// Oracle batch 93 (task 0317): the MRP moby assets (the setup (src/setup/mrp_export.cpp) -> assets/generated/mrp/
// moby/level<N>/<class>.mrpm, src/formats/moby_mrp.h) against the faithful path, on the level's own
// classes in memory (the level loaded as the game loads it; --level N for another level).
//
//   mrp_static   every class packet the level holds (the armor's too): the asset's raw bytes equal
//                the game's copy (packet entry, VIF list, vertex data; the texture prims as the load
//                0x2400f0 builds them from the asset, VRAM fields at the level's GS RAM base, the
//                frame's TBP0 / TBP1 slots aside), its decoded UVs / index list / prims equal what
//                the VIF list unpacks into VU1 memory (runtime/vu/vu1.cpp's UNPACK); every moby
//                texture's mips 0 / 1 equal the level's copy (0x199a80 descriptors).
//   mrp_skin     the moby pass run on the level's mobys placed around the hero (every loaded class
//                lent a moby in turn; random poses, lights, LODs, sub-mesh colours): per skinning
//                record the joint matrices (scratchpad 0x0 + 0x40 j after 0x250fe0), the light rows
//                (the record's +0x40..+0xbf), the VU0 matrix memory and vertex cache as found, then
//                the asset's records skinned and lit by the CPU reference (mrp::skin_packet) against
//                what 0x1ee858 (+ 0x255870's sub-mesh colours) leaves in each packet's VU1 input.
//   mrp_vu1      the same frames' chains through VU1 with the ported programs (moby_vu1.cpp): per
//                program start the asset's kick program evaluated by the CPU reference
//                (mrp::kick_triangles: the start state's rows, the slots' data, the Q register)
//                against the GS triangles the program kicked (positions, STQ, colour, fog, ADC, the
//                texture registers); the clipping program's runs: the triangles it keeps whole.
//   mrp_capture  the same on the VU1 start states captured in the running game
//                (assets/dumps/oracle/vu1_moby: Veldin's intro / gameplay, Kerwan).
// Debugging: ORACLE_MRP_DIR (the asset root), ORACLE_MRP_FRAMES, ORACLE_MRP_TRACE (the chain, the
// mobys drawn, the first differences), ORACLE_MRP_BISECT (the moby the draw loop fails on).
// mrp_scene_asset reads the scene's unpacks: run the batch with RAC1_PERF_MOBY_PARTS=0 for it (by default the scene's
// parts come from the draw loop's notes, which carry none, and the check is skipped).

#include "ovl_oracle.h"

#include <formats/moby_mrp.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/moby_draw.h>
#include <game/ovl/moby_joints.h>
#include <game/ovl/moby_skin.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/moby_vu0.h>
#include <game/ovl/moby_vu1.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/vu0_macro.h>
#include <game/math/vecmath.h>
#include <runtime/vu/vu0.h>
#include <runtime/vu/vu1.h>
#include <game/render/mrp_scene.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {

std::vector<uint8_t> g_level_snap;
bool g_frame_failed = false;

void level_state() {
    uint8_t* ram = ee_get_ram_base();
    if (g_level_snap.empty()) {
        veldin_boot();
        particle_table_init();
        level_setup_body(1, 0);
        w32(OV(0x0016c060), 0xf);
        // The sound listener's six points (0x13f2f0..) are cached per frame (0x13f2e8 = the frame counter 0x15f5cc when
        // refreshed): a snapshot's first frame has the counter 0 = the cache's 0, so the points stay (0, 0, 0) and a
        // level whose first frame has a sound playing (level 18) sends a segment from the origin across the whole level
        // (89 x 155 x 23 cells: the cell list overflows the scratchpad in 0x1efc70, the original's too: task 0323).
        // In the game the counter has been incremented before the first sound frame: the cache is stale.
        w32(0x0013f2e8, 0xffffffffu);
        try {
            gameplay_frame();
        } catch (const std::exception& e) {  // (Veldin's finale: the ported frame's collision reads past memory in the oracle)
            std::printf("  (level %d: the first gameplay frame fails in the oracle: %s; the frames' checks skipped)\n", g_oracle_level, e.what());
            g_frame_failed = true;
        }
        w16(0x001518d0, 512);
        w16(0x001518d2, 416);
        screen_setup();
        projection_setup();
        render_view_setup();
        g_level_snap.assign(ram, ram + 0x02000000);
    } else {
        std::memcpy(ram, g_level_snap.data(), g_level_snap.size());
    }
}

std::string asset_dir() {
    const char* d = std::getenv("ORACLE_MRP_DIR");
    return (d != nullptr ? std::string(d) : std::string("assets/generated/mrp/moby")) + "/level" + std::to_string(g_oracle_level);
}

// The level's classes in memory (0x197300: class header by o_class) and their assets.
struct LevelClasses {
    std::map<uint32_t, uint32_t> header;                 // o_class -> EE address
    std::map<uint32_t, std::unique_ptr<mrp::MobyClass>> asset;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> by_vif, by_vdata;  // EE address -> (o_class, packet)
    std::vector<uint32_t> unmatched;  // class headers in memory with geometry and no asset
};

LevelClasses& classes() {
    static LevelClasses lc;
    static bool done = false;
    if (done) return lc;
    done = true;
    // 0x197300: the loaded classes' headers by class index, 0x197a00: their o_class (0x2409b8)
    for (uint32_t cb = 0; cb < lwu(OV(0x0015ffc0)) && cb < 0x100; ++cb) {
        const uint32_t cls = ee_read<uint32_t>(OV(0x00197300) + cb * 4);
        if (cls == 0 || cls >= 0x02000000u) continue;
        const uint32_t pt = ee_read<uint32_t>(cls);
        if (pt == 0 || pt >= 0x02000000u) continue;
        const uint32_t oc = ee_read<uint16_t>(OV(0x00197a00) + cb * 2);
        auto a = std::make_unique<mrp::MobyClass>();
        bool found = mrp::read_class(asset_dir() + "/" + std::to_string(oc) + ".mrpm", *a) && !a->packets.empty();
        if (!found) {  // the hero's armor (0x23fa00): matched by its first packet's vertex data
            const std::string ad = asset_dir() + "/../armor";
            if (std::filesystem::is_directory(ad))
                for (const auto& de : std::filesystem::directory_iterator(ad)) {
                    if (de.path().extension() != ".mrpm" || !mrp::read_class(de.path().string(), *a) || a->packets.empty()) continue;
                    const uint32_t vd = ee_read<uint32_t>(pt + 8);
                    bool same = vd < 0x02000000u && ee_read<uint8_t>(pt + 0xc) * 16u == a->packets[0].vdata.size();
                    for (size_t b = 0; same && b < a->packets[0].vdata.size(); ++b) same = ee_read<uint8_t>(vd + static_cast<uint32_t>(b)) == a->packets[0].vdata[b];
                    if (same) {
                        a->o_class = static_cast<int32_t>(oc);
                        found = true;
                        break;
                    }
                }
        }
        if (!found) {
            lc.unmatched.push_back(cls);
            std::printf("  (class index %u: o_class %u at %08x)\n", cb, oc, cls);
            continue;
        }
        lc.header[oc] = cls;
        for (uint32_t k = 0; k < a->packets.size(); ++k) {
            lc.by_vif[ee_read<uint32_t>(pt + k * 0x10)] = {oc, k};
            lc.by_vdata[ee_read<uint32_t>(pt + k * 0x10 + 8)] = {oc, k};
        }
        lc.asset[oc] = std::move(a);
    }
    return lc;
}

uint32_t f2u(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
float u2f(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }

// ---------------------------------------------------------------- mrp_static

void check_static() {
    level_state();
    LevelClasses& lc = classes();
    int n_cls = 0, n_pk = 0, bad = 0, missing = 0, shown = 0;
    uint64_t n_bytes = 0;
    auto fail = [&](uint32_t oc, uint32_t k, const char* what) {
        ++bad;
        if (shown++ < 10) std::printf("  mrp_static: class %u packet %u: %s\n", oc, k, what);
    };
    for (uint32_t cls : lc.unmatched) {
        ++missing;
        if (shown++ < 30) std::printf("  mrp_static: class header %08x in memory matches no asset (hdr %08x %08x)\n", cls, ee_read<uint32_t>(cls + 4), ee_read<uint32_t>(cls + 8));
    }
    for (const auto& [oc, cls] : lc.header) {
        auto it = lc.asset.find(oc);
        if (it == lc.asset.end()) {
            ++missing;
            if (shown++ < 30) std::printf("  mrp_static: class %u in memory (%08x) has no asset\n", oc, cls);
            continue;
        }
        const mrp::MobyClass& a = *it->second;
        ++n_cls;
        // the header (pointers as offsets in the asset)
        for (uint32_t b = 0; b < 0x48; ++b) {
            // (pointers; +0x2c..0x2f the game rewrites at load)
            const bool ptr = b < 4 || (b >= 0x10 && b < 0x24) || (b >= 0x28 && b < 0x30);
            if (ptr) continue;
            if (((ee_read<uint8_t>(cls + b) ^ a.header[b]) & (b == 0x44 ? 0xdf : 0xff)) != 0) {  // (+0x44 bit 5: set at run time)
                if (shown < 10) std::printf("  header byte %x: memory %02x asset %02x\n", b, ee_read<uint8_t>(cls + b), a.header[b]);
                fail(oc, 0, "class header differs");
                break;
            }
        }
        const uint32_t pt = ee_read<uint32_t>(cls);
        for (uint32_t k = 0; k < a.packets.size(); ++k) {
            const mrp::Packet& p = a.packets[k];
            ++n_pk;
            const uint32_t e = pt + k * 0x10;
            const uint32_t vif = ee_read<uint32_t>(e), vd = ee_read<uint32_t>(e + 8);
            bool ok = ee_read<uint16_t>(e + 4) * 16u == p.vif.size() && ee_read<uint8_t>(e + 0xc) * 16u == p.vdata.size();
            for (uint32_t b = 4; b < 16 && ok; ++b)
                if (b < 6 || b >= 12) ok = ee_read<uint8_t>(e + b) == p.entry[b];  // (+6: the level load clears it)
            if (!ok) {
                if (shown < 10) {
                    std::printf("  entry memory");
                    for (uint32_t b = 0; b < 16; ++b) std::printf(" %02x", ee_read<uint8_t>(e + b));
                    std::printf(" asset");
                    for (uint32_t b = 0; b < 16; ++b) std::printf(" %02x", p.entry[b]);
                    std::printf(" sizes %zu %zu\n", p.vif.size(), p.vdata.size());
                }
                fail(oc, k, "packet entry differs");
                continue;
            }
            // the VIF list as the level load leaves it: the asset's bytes with each texture prim's
            // data replaced by the GS registers the load built (VRAM fields at the level's GS RAM
            // base); TEX0 / MIPTBP1's TBP0 / TBP1 are the frame's slots (0x24fad0): not compared
            std::vector<uint8_t> want = p.vif;
            std::vector<uint8_t> mask(p.vif.size(), 0xff);
            const int32_t vbase = lw(0x0015ee8c) >> 8;
            for (size_t t = 0; t < p.textures.size(); ++t) {
                const mrp::TexturePrim& tp = p.textures[t];
                const size_t q0 = p.vif_tex_off + t * 4 * 16;
                if (!tp.gs_known) {
                    for (int r = 0; r < 4; ++r) std::memset(&mask[q0 + static_cast<size_t>(r) * 16], 0, 8);
                    continue;
                }
                uint64_t g[4];
                mrp::prim_gs(tp, vbase, g);
                for (int r = 0; r < 4; ++r) std::memcpy(&want[q0 + static_cast<size_t>(r) * 16], &g[r], 8);
                for (int r = 2; r < 4; ++r) {
                    mask[q0 + static_cast<size_t>(r) * 16] = 0;
                    mask[q0 + static_cast<size_t>(r) * 16 + 1] &= 0xc0;
                }
            }
            for (size_t b = 0; b < p.vif.size() && ok; ++b) ok = ((ee_read<uint8_t>(vif + static_cast<uint32_t>(b)) ^ want[b]) & mask[b]) == 0;
            if (!ok) {
                if (shown < 6)
                    for (size_t q = 0; q < p.vif.size(); q += 4) {
                        uint32_t wm = ee_read<uint32_t>(vif + static_cast<uint32_t>(q)), wa;
                        std::memcpy(&wa, &want[q], 4);
                        if (wm != wa) std::printf("    +%03zx memory %08x asset %08x (tex at +%03x)\n", q, wm, wa, p.vif_tex_off);
                    }
                fail(oc, k, "VIF list differs");
                continue;
            }
            for (size_t b = 0; b < p.vdata.size() && ok; ++b) ok = ee_read<uint8_t>(vd + static_cast<uint32_t>(b)) == p.vdata[b];
            if (!ok) {
                fail(oc, k, "vertex data differs");
                continue;
            }
            n_bytes += p.vif.size() + p.vdata.size();
            // the VIF list into a VU1 (TOPS = 0x164, the chain's row / mask: z = 0x1000 from the row)
            vu1::Vu1 vu;
            vu.tops = 0x164;
            vu.mask = 0;  // (the decoded lanes only)
            for (size_t i = 0; i + 4 <= want.size();) {
                uint32_t code;
                std::memcpy(&code, &want[i], 4);
                i += 4;
                const uint32_t nb = vu1::vif_data_bytes(vu, code);
                vu1::vif1_command(vu, code & ~0x10000000u, want.data() + i, nullptr);  // (unmasked: the decoded lanes)
                i += nb;
            }
            auto word = [&](uint32_t qw, int lane) {
                int32_t w;
                std::memcpy(&w, &vu.st.mem[((qw & 0x3ff) << 4) + 4 * static_cast<uint32_t>(lane)], 4);
                return w;
            };
            const uint32_t B = 0x164;
            for (size_t s = 0; s < p.uv.size() && ok; ++s)
                ok = word(B + p.vif_uv_addr + static_cast<uint32_t>(s), 0) == p.uv[s][0] && word(B + p.vif_uv_addr + static_cast<uint32_t>(s), 1) == p.uv[s][1];
            if (p.lod == 2 ? (!p.uv.empty() && p.vif_uv_addr != 194) : (p.vif_uv_addr != 194 || p.uv.size() != p.n_slots)) ok = false;  // (the shadow mesh: no UVs)
            if (!ok) {
                std::printf("    (lod %u, uv addr %u, %zu uvs, slots %u)\n", p.lod, p.vif_uv_addr, p.uv.size(), p.n_slots);
                fail(oc, k, "UVs differ from the VIF unpack");
                continue;
            }
            if (p.vif_index_addr != 301) ok = false;
            for (int l = 0; l < 4 && ok; ++l) ok = word(B + 301, l) == p.index_header[l];
            for (size_t j = 0; j < p.indices.size() && ok; ++j) ok = word(B + 302 + static_cast<uint32_t>(j / 4), static_cast<int>(j % 4)) == p.indices[j];
            if (!ok) {
                fail(oc, k, "index list differs from the VIF unpack");
                continue;
            }
            if (!p.textures.empty() && p.vif_tex_addr != 301 + static_cast<uint32_t>(p.index_header[1])) ok = false;
            for (size_t t = 0; t < p.textures.size() && ok; ++t)
                for (int r = 0; r < 4 && ok; ++r) {
                    const uint32_t q = B + p.vif_tex_addr + static_cast<uint32_t>(t * 4) + static_cast<uint32_t>(r);
                    const uint64_t d = static_cast<uint32_t>(word(q, 0)) | static_cast<uint64_t>(static_cast<uint32_t>(word(q, 1))) << 32;
                    uint64_t w0;
                    std::memcpy(&w0, &want[p.vif_tex_off + (t * 4 + static_cast<size_t>(r)) * 16], 8);
                    ok = d == w0 && static_cast<int32_t>(u2f(static_cast<uint32_t>(word(q, 2)))) == p.textures[t].reg[r];
                    if (r == 0 && ok) ok = static_cast<int8_t>(word(q, 3)) == p.textures[t].next_secret;
                }
            if (!ok) {
                fail(oc, k, "texture prims differ from the VIF unpack");
                continue;
            }
        }
    }
    // the moby textures: the level's descriptors (0x199a80: log2 side << 28 | data address)
    std::vector<mrp::Texture> tex;
    int n_tex = 0, bad_tex = 0;
    if (mrp::read_textures(asset_dir() + "/textures.mrpt", tex)) {
        for (const mrp::Texture& t : tex) {
            const uint32_t d = ee_read<uint32_t>(OV(0x00199a80) + t.index * 4);
            if (d == 0) continue;
            const uint32_t log2 = d >> 28, data = (d << 4) >> 4;
            bool ok = t.mips.size() >= 2 && (1u << log2) == t.width && t.width == t.height;
            uint32_t a = data;
            for (size_t m = 0; m < 2 && ok; ++m) {  // (mips 2 / 3: the GS RAM block's, not in EE memory)
                for (size_t b = 0; b < t.mips[m].size() && ok; ++b) ok = ee_read<uint8_t>(a + static_cast<uint32_t>(b)) == t.mips[m][b];
                a += static_cast<uint32_t>(t.mips[m].size());
            }
            ++n_tex;
            if (!ok) {
                ++bad_tex;
                if (shown++ < 10) std::printf("  mrp_static: texture %u (%ux%u) differs from the level's copy (descriptor %08x)\n", t.index, t.width, t.height, d);
            }
        }
    } else {
        ++bad_tex;
        std::printf("  mrp_static: no textures.mrpt\n");
    }
    const bool ok = bad == 0 && missing == 0 && bad_tex == 0 && n_cls > 0;
    std::printf("%-26s %s  %s (classes %d, packets %d, %llu bytes, textures %d; %d differ, %d without asset, %d textures differ)\n", "mrp_static", "(asset)",
                ok ? "ok" : "FAIL", n_cls, n_pk, static_cast<unsigned long long>(n_bytes), n_tex, bad, missing, bad_tex);
    if (!ok) ++g_fail;
}

// ---------------------------------------------------------------- the frames

// The level's mobys put in front of the camera at random distances (both LODs), random poses
// (keys, blend channels, additive layers over the class's joints), light sets and colours.
void place_mobys() {
    level_state();
    const float cx = ee_read<float>(OV(0x00166dc0)), cy = ee_read<float>(OV(0x00166dc4)), cz = ee_read<float>(OV(0x00166dc8));
    float fw[3], rt[3], up[3];
    for (int k = 0; k < 3; ++k) {
        rt[k] = ee_read<float>(OV(0x00166c80) + 0x10 * u(k));
        up[k] = ee_read<float>(OV(0x00166c84) + 0x10 * u(k));
        fw[k] = ee_read<float>(OV(0x00166c88) + 0x10 * u(k));
    }
    const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
    uint32_t n = 0;
    while (n < 2000 && ee_read<uint8_t>(first + n * 0x100 + 0x20) != 0xff) ++n;
    float hero[3];
    for (uint32_t k = 0; k < 3; ++k) hero[k] = ee_read<float>(first + 4 * k);  // (the first moby: the hero)
    if (std::getenv("ORACLE_MRP_TRACE") != nullptr) {
        int alive = 0, geo = 0;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t m = first + i * 0x100, c = ee_read<uint32_t>(m + 0x24);
            if (ee_read<int8_t>(m + 0x20) >= 0) ++alive;
            if (ee_read<int8_t>(m + 0x20) >= 0 && c != 0 && ee_read<uint32_t>(c) != 0) ++geo;
        }
        std::printf("  mobys %u (first %08x), alive %d, with geometry %d\n", n, first, alive, geo);
    }
    // every loaded class drawn in turn: mobys lent to the classes (their class pointer, class
    // byte, a first frame of the class's first sequence as both animation keys)
    static size_t cursor = 0;
    std::vector<std::pair<uint32_t, uint32_t>> targets;  // (class header, class index)
    for (uint32_t cb = 0; cb < lwu(OV(0x0015ffc0)) && cb < 0x100; ++cb) {
        const uint32_t c = ee_read<uint32_t>(OV(0x00197300) + cb * 4);
        if (c == 0 || c >= 0x02000000u || ee_read<uint32_t>(c) == 0) continue;
        if (ee_read<uint8_t>(c + 4) == 0 || (ee_read<uint8_t>(c + 4) > 100 && (ee_read<uint8_t>(c + 5) == 0 || ee_read<uint8_t>(c + 5) > 100))) continue;  // (no low LOD: kept high, +0x72)
        uint32_t frame = 0;
        if (ee_read<uint8_t>(c + 8) != 0 || ee_read<uint8_t>(c + 9) != 0) {
            for (uint32_t q = 0; q < ee_read<uint8_t>(c + 0xc) && frame == 0; ++q) {
                const uint32_t sq = ee_read<uint32_t>(c + 0x48 + 4 * q);
                if (sq != 0 && sq < 0x02000000u && ee_read<uint8_t>(sq + 0x10) != 0) frame = ee_read<uint32_t>(sq + 0x1c);
            }
            if (frame == 0) continue;
        }
        targets.push_back({c, cb});
    }
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t m = first + i * 0x100;
        if (ee_read<int8_t>(m + 0x20) < 0 || ee_read<uint32_t>(m + 0x24) == 0) continue;
        if (targets.empty() || ri(0, 1) == 0) continue;
        const auto [c, cb] = targets[cursor++ % targets.size()];
        w32(m + 0x24, c);
        w8(m + 0x22, cb);
        w32(m + 0x60, 0);  // (no blend channels / layers of the moby's own class)
        w32(m + 0x64, 0);
        if (ee_read<uint8_t>(c + 8) != 0 || ee_read<uint8_t>(c + 9) != 0) {
            uint32_t frame = 0;
            for (uint32_t q = 0; q < ee_read<uint8_t>(c + 0xc) && frame == 0; ++q) {
                const uint32_t sq = ee_read<uint32_t>(c + 0x48 + 4 * q);
                if (sq != 0 && sq < 0x02000000u && ee_read<uint8_t>(sq + 0x10) != 0) frame = ee_read<uint32_t>(sq + 0x1c);
            }
            w32(m + 0x68, frame);
            w32(m + 0x6c, frame);
            w32(m + 0x50, 0);
            wf(m + 0x54, 0.0f);
        }
    }
    // (at most 90 mobys in view, the rest hidden: about what a frame of the game draws -- more
    // overruns the skinning ring into the packet buffer, which the game never does)
    const bool nomove = std::getenv("ORACLE_MRP_NOMOVE") != nullptr;
    int in_view = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t m = first + i * 0x100;
        const uint32_t cls = ee_read<uint32_t>(m + 0x24);
        if (ee_read<int8_t>(m + 0x20) < 0) continue;
        if (cls == 0 || cls >= 0x02000000u || ee_read<uint32_t>(cls) == 0) {  // (no class / no geometry: not drawn)
            w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 0x80);
            continue;
        }
        if (ee_read<uint8_t>(cls + 4) == 0) {  // (no packets at the high LOD: the draw loop's packet loop would not end; not drawn)
            w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 0x80);
            continue;
        }
        w16(m + 0x32, 0x7fff);  // (the draw distance)
        const bool show = !nomove && in_view < 90 && ri(0, 3) != 0;
        if (show) {
            ++in_view;
            // (most in view; some across the screen's edges or very near: the clipping program)
            const int where = ri(0, 5);
            // (around the hero, whom the level's start camera frames: along the view axis)
            const float d = where == 0 ? rf(-12.0f, -4.0f) : rf(-2, 50), side = (where == 1 ? rf(0.5f, 1.2f) : rf(0.0f, 0.35f)) * (d + 8.0f);
            const float x = rf(-side, side), y = rf(-side, side);
            for (uint32_t k = 0; k < 3; ++k) wf(m + 4 * k, hero[k] + fw[k] * d + rt[k] * x + up[k] * y);
            for (uint32_t k = 0; k < 3; ++k) wf(m + 0x10 + 4 * k, ee_read<float>(m + 4 * k));  // (+0x10: the translation the draw uses)
        }
        uint32_t fl = ee_read<uint16_t>(m + 0x34) & ~0x0e99u;  // (0x80 / 1: not drawn; 0xc00 / 0x8 / 0x200: shadows, fades)
        w8(m + 0x36, 0xff);  // (the visibility group bits: every group seen)
        if (ri(0, 4) == 0) fl |= 0x10;  // the sub-mesh colour
        if (!show && !nomove) fl |= 0x80;
        w16(m + 0x34, fl);
        w8(m + 0x31, u(ri(0, 1)));
        w8(m + 0x72, ee_read<uint8_t>(cls + 5) == 0 ? 255u : ee_read<uint8_t>(cls + 4) > 100 ? 0u : u(ri(0, 40)));  // the LOD distance (no low LOD: always high; over 100 high packets, the skinning record's 0x400 bytes: always low)
        if (ee_read<uint8_t>(cls + 4) > 100 && (ee_read<uint8_t>(cls + 5) == 0 || ee_read<uint8_t>(cls + 5) > 100)) w16(m + 0x34, ee_read<uint16_t>(m + 0x34) | 0x80);
        w8(m + 0x73, 0);             // (no shadow mesh)
        w8(m + 0x23, u(ri(0, 255)));
        if (ri(0, 7) == 0) {  // (big and near: across the guard band, the clipping program)
            wf(m + 0x2c, rf(4.0f, 12.0f));
            const float d = rf(2.0f, 15.0f);
            for (uint32_t k = 0; k < 3; ++k) {
                wf(m + 4 * k, hero[k] + fw[k] * d + rt[k] * rf(-2.0f, 2.0f));
                wf(m + 0x10 + 4 * k, ee_read<float>(m + 4 * k));
            }
        }
        else if (coin()) wf(m + 0x2c, rf(0.5f, 2.0f));
        w8(m + 0x38, u(ri(0, 15)));
        w8(m + 0x39, u(ri(0, 15)));
        w8(m + 0x3a, coin() ? 0u : u(ri(0, 255)));
        if (coin()) w32(m + 0x3c, R());
        w32(m + 0x90, R());  // (the sub-mesh colour)
        const uint32_t nj = std::min(ee_read<uint8_t>(cls + 8), ee_read<uint8_t>(cls + 9));
        if (nj != 0) {
            if (ee_read<uint32_t>(m + 0x6c) != 0 && coin()) wf(m + 0x54, rf(0.01f, 0.99f));
            const uint32_t area = 0x01c00000u + (i & 0x1ff) * 0x400;
            auto joint = [nj] { return 0x70000000u + u(ri(0, static_cast<int32_t>(nj) - 1)) * 0x40; };
            auto quat = [](uint32_t a) {
                float q[4], l = 0;
                for (float& x : q) { x = rf(-1, 1); l += x * x; }
                l = std::sqrt(l) + 1e-3f;
                for (uint32_t k = 0; k < 4; ++k) wf(a + 4 * k, q[k] / l);
            };
            w32(m + 0x60, 0);
            w32(m + 0x64, 0);
            // (the random channels / layers' nodes at 0x1c00000: free on Veldin only; on the other
            // levels the mobys keep their own animation keys)
            if (g_oracle_level == 0 && ri(0, 2) == 0) {  // blend channels
                uint32_t prev = m + 0x60 - 0x1c;
                for (uint32_t c = 0, nc = u(ri(1, 2)); c < nc; ++c) {
                    const uint32_t node = area + c * 0x100, data = node + 0x20;
                    const uint32_t cnt = u(ri(1, 4));
                    w32(prev + 0x1c, node);
                    w16(node + 2, cnt);
                    wf(node + 8, rf(0, 1));
                    w32(node + 0x10, data);
                    w32(node + 0x1c, 0);
                    for (uint32_t e = 0; e < cnt; ++e) {
                        quat(data + e * 0x30);
                        wfrand(data + e * 0x30 + 0x10, 4, 0.5f, 1.5f);
                        wfrand(data + e * 0x30 + 0x20, 3, -2, 2);
                        w32(data + e * 0x30 + 0x2c, joint());
                    }
                    prev = node;
                }
            }
            if (g_oracle_level == 0 && ri(0, 2) == 0) {  // additive layers
                uint32_t prev = m + 0x64 - 8;
                for (uint32_t c = 0, nc = u(ri(1, 3)); c < nc; ++c) {
                    const uint32_t node = area + 0x200 + c * 0x40;
                    w32(prev + 8, node);
                    w8(node + 3, coin() ? 0u : 1u);
                    w32(node + 4, joint());
                    w32(node + 8, 0);
                    wf(node + 0xc, rf(0, 1));
                    quat(node + 0x10);
                    wfrand(node + 0x20, 4, 0.5f, 1.5f);
                    wfrand(node + 0x30, 4, -2, 2);
                    prev = node;
                }
            }
        }
    }
    // the light block: 16 directional sets, 8 point lights (some unset)
    for (uint32_t s = 0; s < 16; ++s) {
        const uint32_t a = OV(0x0017fec0) + s * 0x40;
        wfrand(a, 16, 0.0f, 1.0f);
        wfrand(a + 0x10, 3, -1.0f, 1.0f);
        wfrand(a + 0x30, 3, -1.0f, 1.0f);
    }
    for (uint32_t p = 0; p < 8; ++p) {
        const uint32_t a = OV(0x0017fec0) + 0x400 + p * 0x20;
        wfrand(a, 4, 0.0f, 1.0f);
        if (ri(0, 2) == 0) {
            w32(a + 0x10, 0);
            w32(a + 0x14, 0);
        } else {
            const float d = rf(0, 60);
            for (uint32_t k = 0; k < 3; ++k) wf(a + 0x10 + 4 * k, (k == 0 ? cx : k == 1 ? cy : cz) + fw[k] * d + rf(-15, 15));
            wf(a + 0x1c, rf(0.5f, 30.0f));
        }
    }
    for (uint32_t i = 0; i < 0x80; ++i) w8(OV(0x00173d00) + i, 0xffu);  // every group seen
    // (the packet buffer, the skinning ring and work buffer: the level's own, 0x1611c0 / 0x15f5f8 / 0x15f5fc)
}

// One skinning record as 0x2558e0 handles it, with what the reference needs.
struct Record {
    uint32_t addr = 0;
    std::vector<std::pair<uint32_t, uint32_t>> packets;  // (o_class, packet) in order
    std::vector<float> joints;                           // scratchpad 0x0..0x2000 after the pose
    std::array<std::array<float, 4>, 8> light{};         // the record's +0x40..+0xbf
    std::vector<uint8_t> vu0_mem, cache;                 // VU0 data memory, the vertex cache (work) before the skinning
    std::vector<int64_t> tint;                           // per packet: the sub-mesh colour 0x255870 writes over its colours (-1: none)
};

// One VU1 program start of the moby chain.
struct Run {
    uint32_t start = 0;
    VUState st;                              // at the start (after the double buffer swap)
    std::vector<std::vector<uint8_t>> kicks; // the GIF packets it kicked
    uint32_t vif = 0;                        // the last REF'd VIF list before it
};

struct Frame {
    std::vector<float> normals;  // the scratchpad's normal table (0x3800, from 0x166080)
    std::vector<Record> records;
    std::vector<Run> runs;
};

uint32_t kWork = 0, kRing = 0;

bool run_frame(Frame& f) {
    place_mobys();
    vu0m::regs() = VUState{};
    vu0m::regs().vf[0].f[3] = 1.0f;
    vu0::vif0_send_chain(0x0010e4c0, true);
    sub_002a07f8();  // (the game's own packet buffers, ring and work buffer: 0x2a07f8, then a flip 0x2a0858)
    sub_002a0858();
    kRing = lwu(OV(0x0015f5f8));
    kWork = lwu(OV(0x0015f5fc));
    if (std::getenv("ORACLE_MRP_TRACE") != nullptr) std::printf("  ring %08x work %08x chain %08x limit %08x\n", kRing, kWork, lwu(OV(0x001611c0)), lwu(OV(0x001611c8)));
    w32(OV(0x0016a018), 1);
    w32(OV(0x0016a014), 1);
    const uint32_t chain0 = lwu(OV(0x001611c0));
    moby_pass_start();
    spr_moby_fill();
    if (std::getenv("ORACLE_MRP_BISECT") != nullptr) {  // (debugging: the moby the draw loop fails on)
        std::vector<uint8_t> save(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
        std::vector<uint8_t> spr(0x4000);
        for (uint32_t i = 0; i < 0x4000; ++i) spr[i] = ee_read<uint8_t>(0x70000000u + i);
        const uint32_t first = lwu(OV(0x0015ffd8));
        for (int32_t c = 1; c < 400; ++c) {
            std::memcpy(ee_get_ram_base(), save.data(), save.size());
            for (uint32_t i = 0; i < 0x4000; ++i) w8(0x70000000u + i, spr[i]);
            try {
                moby_draw_loop(first, lwu(OV(0x0015ffd4)), c, 1);
            } catch (const std::exception&) {
                const uint32_t m = first + static_cast<uint32_t>(c - 1) * 0x100, cl = ee_read<uint32_t>(m + 0x24);
                std::printf("  draw loop fails at moby %d (%08x): class %08x (o_class %d) counts %u %u %u %u +34 %04x +72 %u\n", c - 1, m, cl, ee_read<int16_t>(m + 0xa6),
                            ee_read<uint8_t>(cl + 4), ee_read<uint8_t>(cl + 5), ee_read<uint8_t>(cl + 6), ee_read<uint8_t>(cl + 7), ee_read<uint16_t>(m + 0x34), ee_read<uint8_t>(m + 0x72));
                break;
            }
        }
        std::memcpy(ee_get_ram_base(), save.data(), save.size());
        for (uint32_t i = 0; i < 0x4000; ++i) w8(0x70000000u + i, spr[i]);
    }
    const uint32_t r = moby_draw_loop(lwu(OV(0x0015ffd8)), lwu(OV(0x0015ffd4)), -1, 1);
    sw(OV(0x0015ffd4), r);
    if (std::getenv("ORACLE_MRP_TRACE") != nullptr) {
        const float cx = ee_read<float>(OV(0x00166dc0)), cy = ee_read<float>(OV(0x00166dc4)), cz = ee_read<float>(OV(0x00166dc8));
        int shown = 0, drawn = 0;
        for (uint32_t m = lwu(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
            if (ee_read<uint8_t>(m + 0x31) != 0) {
                ++drawn;
                continue;
            }
            if (shown++ < 6) {
                float d[3];
                for (uint32_t k = 0; k < 3; ++k) {
                    const float dx = ee_read<float>(m) - cx, dy = ee_read<float>(m + 4) - cy, dz = ee_read<float>(m + 8) - cz;
                    d[k] = dx * ee_read<float>(OV(0x00166c80) + 4 * k) + dy * ee_read<float>(OV(0x00166c90) + 4 * k) + dz * ee_read<float>(OV(0x00166ca0) + 4 * k);
                }
                std::printf("  not drawn: moby %08x class %08x +34 %04x +32 %d +36 %02x +37 %02x +20 %d view (%g %g %g)\n", m, ee_read<uint32_t>(m + 0x24), ee_read<uint16_t>(m + 0x34),
                            ee_read<int16_t>(m + 0x32), ee_read<uint8_t>(m + 0x36), ee_read<uint8_t>(m + 0x37), ee_read<int8_t>(m + 0x20), d[0], d[1], d[2]);
            }
        }
        std::printf("  drawn %d, max distance %d, camera (%g %g %g %g)\n", drawn, lw(OV(0x0015fff0)), cx, cy, cz, ee_read<float>(OV(0x00166dcc)));
    }
    prof_mark_end(OV(0x0015ffa0), 3);
    moby_textures_chain();
    // 0x24fd80 / 0x2558e0 with the reference's inputs taken between the pose and the skinning
    mem_copy128(0x70003800, OV(0x00166080), 0x800);
    f.normals.resize(0x200);
    for (uint32_t i = 0; i < 0x200; ++i) f.normals[i] = ee_read<float>(0x70003800u + 4 * i);
    LevelClasses& lc = classes();
    for (uint32_t rec = kRing; lw(rec) != 0;) {
        Record R;
        R.addr = rec;
        for (uint32_t e = rec + 0xc0; ee_read<uint64_t>(e) != 0; e += 8) {
            const uint32_t vd = (ee_read<uint32_t>(e) >> 8) << 4;
            auto it = lc.by_vdata.find(vd);
            R.packets.push_back(it != lc.by_vdata.end() ? it->second : std::pair<uint32_t, uint32_t>{0xffffffffu, 0});
            R.tint.push_back(-1);
        }
        moby_joints_pose(rec, kWork);
        R.joints.resize(0x800);
        for (uint32_t i = 0; i < 0x800; ++i) R.joints[i] = ee_read<float>(0x70000000u + 4 * i);
        for (uint32_t q = 0; q < 8; ++q)
            for (uint32_t l = 0; l < 4; ++l) R.light[q][l] = ee_read<float>(rec + 0x40 + q * 0x10 + l * 4);
        R.vu0_mem.assign(vu0::global().st.mem.begin(), vu0::global().st.mem.begin() + 0x1000);
        R.cache.resize(0x2000);
        for (uint32_t i = 0; i < 0x2000; ++i) R.cache[i] = ee_read<uint8_t>(kWork + i);
        f.records.push_back(std::move(R));
        rec = moby_skin(rec, kWork);
    }
    // the sub-mesh colours (0x1ac200: colour, class packet, the record's entries, count)
    for (uint32_t at = OV(0x001ac200), e = at + lwu(OV(0x0015fff8)); at < e; at += 0x10) {
        const uint32_t ent = lwu(at + 8);
        const int32_t n = lw(at + 0xc);
        for (Record& R : f.records)
            for (size_t j = 0; j < R.packets.size(); ++j) {
                const uint32_t ej = R.addr + 0xc0 + static_cast<uint32_t>(j) * 8;
                if (ej >= ent && ej < ent + static_cast<uint32_t>(n) * 8) R.tint[j] = lwu(at);
            }
    }
    if (lw(OV(0x0015fff8)) != 0) moby_submesh_fill();
    // the chain through VU1: the moby programs (MPG 0x10faa0), then the moby chain from its start
    static vu1::Vu1 vu;
    vu = vu1::Vu1{};
    vu.st.vf[0].f[3] = 1.0f;
    Run* cur = nullptr;
    uint32_t last_vif = 0;
    auto kick = [&](const uint8_t* d, uint32_t q) {
        if (cur != nullptr) cur->kicks.emplace_back(d, d + q * 16);
    };
    vu1::g_start_hook = [&](vu1::Vu1& v, uint32_t start, uint64_t, vu1::ProgramFn, const std::function<void(uint32_t)>&) {
        f.runs.emplace_back();
        cur = &f.runs.back();
        cur->start = start;
        cur->st = v.st;
        cur->vif = last_vif;
        return false;
    };
    auto stream = [&](const uint8_t* p, uint32_t n) {
        uint32_t i = 0;
        while (i + 4 <= n) {
            const uint32_t code = static_cast<uint32_t>(p[i]) | static_cast<uint32_t>(p[i + 1]) << 8 | static_cast<uint32_t>(p[i + 2]) << 16 | static_cast<uint32_t>(p[i + 3]) << 24;
            i += 4;
            const uint32_t cmd = (code >> 24) & 0x7f;
            if (cmd == 0x50 || cmd == 0x51) {
                const uint32_t q = (code & 0xffff) == 0 ? 65536 : code & 0xffff;
                i = (i + 15) & ~15u;
                i += q * 16;
                continue;
            }
            const uint32_t nb = vu1::vif_data_bytes(vu, code);
            if (nb == 0 || i + nb <= n) vu1::vif1_command(vu, code, p + i, kick);
            i += nb;
        }
    };
    {
        const uint32_t q = lhu(0x0010fa90);
        std::vector<uint8_t> b(q * 16);
        for (uint32_t k = 0; k < q * 16; ++k) b[k] = ee_read<uint8_t>(0x0010faa0 + k);
        stream(b.data(), q * 16);
    }
    std::vector<uint32_t> stack;
    std::vector<uint8_t> buf;
    uint32_t addr = chain0 & 0x01fffff0u;
    const uint32_t end = lwu(OV(0x001611c0));
    for (int guard = 0; guard < 200000 && addr != end; ++guard) {
        const uint64_t lo = ee_read<uint64_t>(addr);
        const uint32_t qwc = lo & 0xffff, id = (lo >> 28) & 7;
        const uint32_t a = static_cast<uint32_t>(lo >> 32) & 0x01fffff0u;
        uint32_t data = addr + 16, next = addr + 16 + qwc * 16;
        bool stop = false;
        switch (id) {
        case 0: data = a; stop = true; break;
        case 1: break;
        case 2: next = a; break;
        case 3: case 4: data = a; next = addr + 16; break;
        case 5: stack.push_back(next); next = a; break;
        case 6:
            if (stack.empty()) stop = true;
            else { next = stack.back(); stack.pop_back(); }
            break;
        case 7: stop = true; break;
        }
        if (id == 3 && lc.by_vif.count(data) != 0) last_vif = data;
        if (std::getenv("ORACLE_MRP_TRACE") != nullptr && guard < 80) std::printf("  tag %08x: id %u qwc %u addr %08x vif %08x %08x\n", addr, id, qwc, a, ee_read<uint32_t>(addr + 8), ee_read<uint32_t>(addr + 12));
        buf.assign(16, 0);
        for (uint32_t k = 8; k < 16; ++k) buf[k] = ee_read<uint8_t>(addr + k);
        for (uint32_t k = 0; k < qwc * 16; ++k) buf.push_back(ee_read<uint8_t>(data + k));
        stream(buf.data(), static_cast<uint32_t>(buf.size()));
        if (stop) break;
        addr = next;
    }
    vu1::g_start_hook = nullptr;
    return true;
}

// ---------------------------------------------------------------- mrp_skin / mrp_vu1

// The GS side of a program run: its kicked packets as GS triangles (the strip queue restarted by
// a PRIM write; a vertex with ADC closes none) with the texture registers then set.
struct KTri {
    mrp::GsVertex v[3];
    uint64_t tex[4] = {};  // TEX1, CLAMP, TEX0, MIPTBP1 (context 1) at the triangle
    bool tex_set = false;
};
void gs_triangles(const Run& run, std::vector<KTri>& out) {
    mrp::GsVertex q[2];
    int n = 0;
    uint64_t regs[4] = {};
    bool set = false;
    for (const auto& pk : run.kicks) {
        size_t at = 0;
        while (at + 16 <= pk.size()) {
            uint64_t lo, hi;
            std::memcpy(&lo, &pk[at], 8);
            std::memcpy(&hi, &pk[at + 8], 8);
            at += 16;
            const uint32_t nloop = lo & 0x7fff, flg = (lo >> 58) & 3, nreg = ((lo >> 60) & 0xf) == 0 ? 16 : (lo >> 60) & 0xf;
            if ((lo >> 46) & 1) n = 0;  // PRE: PRIM written
            if (flg == 0 && nreg == 1 && (hi & 0xf) == 0xe) {
                for (uint32_t i = 0; i < nloop && at + 16 <= pk.size(); ++i, at += 16) {
                    uint64_t d;
                    std::memcpy(&d, &pk[at], 8);
                    const uint8_t r = pk[at + 8];
                    const int slot = r == 0x14 ? 0 : r == 0x08 ? 1 : r == 0x06 ? 2 : r == 0x34 ? 3 : -1;
                    if (slot >= 0) {
                        regs[slot] = d;
                        set = true;
                    }
                    if (r == 0x00) n = 0;
                }
            } else if (flg == 0 && nreg == 3 && (hi & 0xfff) == 0x412) {
                for (uint32_t i = 0; i < nloop && at + 48 <= pk.size(); ++i, at += 48) {
                    mrp::GsVertex v;
                    std::memcpy(&v.s, &pk[at], 4);
                    std::memcpy(&v.t, &pk[at + 4], 4);
                    std::memcpy(&v.q, &pk[at + 8], 4);
                    std::memcpy(v.rgba, &pk[at + 16], 16);
                    std::memcpy(&v.x, &pk[at + 32], 4);
                    std::memcpy(&v.y, &pk[at + 36], 4);
                    std::memcpy(&v.z, &pk[at + 40], 4);
                    std::memcpy(&v.f, &pk[at + 44], 4);
                    v.adc = ((v.f >> 15) & 1) != 0;
                    if (n >= 2 && !v.adc) {
                        KTri t;
                        t.v[0] = q[0];
                        t.v[1] = q[1];
                        t.v[2] = v;
                        std::memcpy(t.tex, regs, sizeof regs);
                        t.tex_set = set;
                        out.push_back(t);
                    }
                    q[0] = q[1];
                    q[1] = v;
                    n = std::min(n + 1, 2);
                }
            } else {
                const uint32_t body = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
                at += body * 16;
            }
            if ((lo >> 15) & 1) break;  // EOP
        }
    }
}

struct VStats {
    uint64_t runs = 0, tris = 0, tris_ok = 0, ref_missing = 0, extra = 0, tex_bad = 0, untraced = 0;
    uint64_t clip_runs = 0, clip_tris = 0, clip_matched = 0, clip_culled = 0, clip_new = 0;
    std::set<uint32_t> classes;
    int shown = 0;
};
VStats vs;

bool same_vertex(const mrp::GsVertex& a, const mrp::GsVertex& b) {
    // The tolerances: float rounding (the VU rounds toward zero, the host to nearest; the program's
    // operation order is kept by the reference). STQ: 1e-5 relative. x / y (1/16 pixel): +-1, or
    // |v| / 2^16 far off screen. z: the sum p.z Q + offset.z with an offset near 2^24 (an ulp of 1..2,
    // x 16): +-64 or |z| / 1024. All scaled by Q / 0.01 when Q is larger (a vertex at the near
    // plane, w near 0: the rounding of w magnified; mobys the original draws across the camera).
    // Colour and F (with ADC) exact.
    const float amp = std::max(1.0f, std::max(std::fabs(a.q), std::fabs(b.q)) / 0.01f);
    auto rel = [amp](float x, float y) { return std::fabs(x - y) <= 1e-5f * amp * std::max(std::fabs(x), std::fabs(y)) + 1e-30f; };
    // (as signed: far off-screen vertices wrap)
    auto fixd = [amp](uint32_t x, uint32_t y, uint32_t tol) {
        const int64_t a = static_cast<int32_t>(x), b = static_cast<int32_t>(y);
        const int64_t big = std::max(a < 0 ? -a : a, b < 0 ? -b : b) >> 16;
        return static_cast<double>(a > b ? a - b : b - a) <= static_cast<double>(std::max<int64_t>(tol, big)) * amp;
    };
    if (!rel(a.s, b.s) || !rel(a.t, b.t) || !rel(a.q, b.q)) return false;
    for (int l = 0; l < 4; ++l)
        if (a.rgba[l] != b.rgba[l]) return false;
    if (!fixd(a.x, b.x, 1) || !fixd(a.y, b.y, 1)) return false;
    if (!fixd(a.z, b.z, std::max<uint32_t>(64, static_cast<uint32_t>(std::abs(static_cast<int32_t>(a.z)) / 1024)))) return false;
    // (F as the GS reads it: bits 4..11 of the XYZF2 word, ADC bit 15; the clipping program keeps
    // its clip codes in the bits below, which the GS ignores)
    return ((a.f >> 4) & 0xff) == ((b.f >> 4) & 0xff) && a.adc == b.adc;
}

void compare_vu1(VStats& st, const Run& run, const mrp::Packet& p, uint32_t oc, uint32_t k, int32_t vram) {
    const uint32_t B = (run.start == 0x00a || run.start == 0x00e) ? (run.st.xtop & 0x3ff) : 356;
    auto qf = [&](uint32_t qw, int l) {
        float f;
        std::memcpy(&f, &run.st.mem[((qw & 0x3ff) << 4) + 4 * static_cast<uint32_t>(l)], 4);
        return f;
    };
    auto qi = [&](uint32_t qw, int l) {
        int32_t w;
        std::memcpy(&w, &run.st.mem[((qw & 0x3ff) << 4) + 4 * static_cast<uint32_t>(l)], 4);
        return w;
    };
    mrp::Vu1Consts kc;
    for (int r = 0; r < 4; ++r)
        for (int l = 0; l < 4; ++l) kc.rows[r][l] = qf(B + 297 + static_cast<uint32_t>(r), l);
    for (int l = 0; l < 4; ++l) kc.offset[l] = qf(B + 296, l);
    kc.proj = qf(B + 292, 0);
    kc.fog_min = qf(B + 291, 0);
    kc.q_start = run.st.q;
    std::vector<std::array<int32_t, 3>> pos(p.n_slots);
    std::vector<std::array<int32_t, 4>> col(p.n_slots);
    for (uint32_t s = 0; s < p.n_slots; ++s) {
        for (int l = 0; l < 3; ++l) pos[s][static_cast<size_t>(l)] = qi(B + s, l);
        for (int l = 0; l < 4; ++l) col[s][static_cast<size_t>(l)] = qi(B + 97 + s, l);
    }
    std::vector<mrp::GsTriangle> ref;
    if (!p.kicks_ok) {
        if (st.untraced++ < 5) std::printf("  mrp_vu1: class %u packet %u (lod %u) has no kick program (%s)\n", oc, k, p.lod, p.kicks_note.c_str());
        return;
    }
    mrp::kick_triangles(p, kc, pos, col, ref, run.start == 0x00a || run.start == 0x008);
    std::vector<KTri> got;
    gs_triangles(run, got);
    ++st.runs;
    st.classes.insert(oc);
    const bool clip = run.start == 0x00a || run.start == 0x008;
    // (the ADC bit / F of a triangle's first two vertices draw nothing: compared without it)
    auto tri_eq = [](const mrp::GsTriangle& a, const KTri& b) {
        auto open = [](mrp::GsVertex v) {
            v.f &= 0x3fff;
            v.adc = false;
            return v;
        };
        return same_vertex(open(a.v[0]), open(b.v[0])) && same_vertex(open(a.v[1]), open(b.v[1])) && same_vertex(a.v[2], b.v[2]);
    };
    auto tex_ok = [&](const mrp::GsTriangle& a, const KTri& b) {
        if (vram == INT32_MIN) return true;  // (a captured state: its level's GS RAM base not known here)
        if (a.texture < 0 || static_cast<size_t>(a.texture) >= p.textures.size()) return true;  // (the state before the packet)
        uint64_t g[4];
        if (!mrp::prim_gs(p.textures[static_cast<size_t>(a.texture)], vram, g)) return true;
        const uint64_t m[4] = {~0ull, ~0ull, ~0x3fffull, ~0x3fffull};  // (TBP0 / TBP1: the frame's slots)
        for (int r = 0; r < 4; ++r)
            if (((g[r] ^ b.tex[r]) & m[r]) != 0) return false;
        return b.tex_set;
    };
    if (!clip) {
        st.tris += ref.size();
        size_t i = 0;
        for (; i < ref.size() && i < got.size(); ++i) {
            if (!tri_eq(ref[i], got[i])) {
                if (st.shown < 4 && std::getenv("ORACLE_MRP_TRACE") != nullptr) {
                    std::printf("  strip:");
                    for (const auto& op : p.strip) std::printf(" %s%u%s", op.texture >= 0 ? "T" : "", op.slot, op.adc ? "*" : "");
                    std::printf("\n  kicked:");
                    for (const auto& pk : run.kicks) {
                        size_t at = 0;
                        while (at + 16 <= pk.size()) {
                            uint64_t lo;
                            std::memcpy(&lo, &pk[at], 8);
                            const uint32_t nloop = lo & 0x7fff, nreg = ((lo >> 60) & 0xf) == 0 ? 16 : (lo >> 60) & 0xf;
                            at += 16;
                            std::printf(" [%u%s]", nloop, (lo >> 46) & 1 ? "P" : "");
                            if (nreg == 3) {
                                for (uint32_t i = 0; i < nloop; ++i, at += 48) {
                                    int32_t x, y, z;
                                    uint32_t f;
                                    float ss, tt;
                                    std::memcpy(&ss, &pk[at], 4);
                                    std::memcpy(&tt, &pk[at + 4], 4);
                                    std::memcpy(&x, &pk[at + 32], 4);
                                    std::memcpy(&y, &pk[at + 36], 4);
                                    std::memcpy(&f, &pk[at + 44], 4);
                                    // which slot: the reference vertex with these x / y
                                    int slot = -1;
                                    bool loose = false;
                                    for (int pass = 0; pass < 2 && slot < 0; ++pass)
                                        for (uint32_t s2 = 0; s2 < p.n_slots && slot < 0; ++s2) {
                                            const int32_t uv[2] = {p.uv[s2][0], p.uv[s2][1]};
                                            const mrp::GsVertex rv = mrp::vu1_vertex(kc, pos[s2].data(), col[s2].data(), uv, false);
                                            if (rv.x == static_cast<uint32_t>(x) && rv.y == static_cast<uint32_t>(y) && (pass == 1 || (rv.s == ss && rv.t == tt))) {
                                                slot = static_cast<int>(s2) + 1;
                                                loose = pass == 1;
                                            }
                                        }
                                    std::printf(" %d%s%s", slot, loose ? "?" : "", (f >> 15) & 1 ? "*" : "");
                                }
                            } else {
                                at += nloop * nreg * 16;
                            }
                            if ((lo >> 15) & 1) break;
                        }
                    }
                    std::printf("\n");
                }
                if (st.shown++ < 8) {
                    std::printf("  mrp_vu1: class %u packet %u (start %03x) triangle %zu of %zu / %zu (slots %u %u %u) differs\n", oc, k, run.start, i, ref.size(), got.size(), ref[i].slot[0],
                                ref[i].slot[1], ref[i].slot[2]);
                    for (int c = 0; c < 3; ++c)
                        std::printf("    ref  stq %g %g %g rgba %d %d %d %d xyzf %u %u %u %x\n    kick stq %g %g %g rgba %d %d %d %d xyzf %u %u %u %x\n", ref[i].v[c].s, ref[i].v[c].t,
                                    ref[i].v[c].q, ref[i].v[c].rgba[0], ref[i].v[c].rgba[1], ref[i].v[c].rgba[2], ref[i].v[c].rgba[3], ref[i].v[c].x, ref[i].v[c].y, ref[i].v[c].z,
                                    ref[i].v[c].f, got[i].v[c].s, got[i].v[c].t, got[i].v[c].q, got[i].v[c].rgba[0], got[i].v[c].rgba[1], got[i].v[c].rgba[2],
                                    got[i].v[c].rgba[3], got[i].v[c].x, got[i].v[c].y, got[i].v[c].z, got[i].v[c].f);
                }
                break;
            }
            if (!tex_ok(ref[i], got[i])) {
                ++st.tex_bad;
                static int tshown = 0;
                if (tshown++ < 3) {
                    uint64_t g[4];
                    mrp::prim_gs(p.textures[static_cast<size_t>(ref[i].texture)], vram, g);
                    std::printf("  mrp_vu1: class %u packet %u triangle %zu: texture registers differ:", oc, k, i);
                    for (int r = 0; r < 4; ++r) std::printf(" %016llx/%016llx", static_cast<unsigned long long>(g[r]), static_cast<unsigned long long>(got[i].tex[r]));
                    const mrp::TexturePrim& tp = p.textures[static_cast<size_t>(ref[i].texture)];
                    std::printf("\n    prim %d of %zu: texture %d local %d known %d entry %d file", ref[i].texture, p.textures.size(), tp.texture, tp.local_texture, tp.gs_known, tp.has_entry);
                    for (int r = 0; r < 4; ++r) std::printf(" %016llx", static_cast<unsigned long long>(tp.file[r]));
                    std::printf(" vram %d\n", vram);
                }
            }
            ++st.tris_ok;
        }
        st.ref_missing += ref.size() - i;
        if (got.size() > i) st.extra += got.size() - i;
        return;
    }
    // the clipping program: triangles crossing the guard band are clipped (new vertices) or culled;
    // every kicked triangle made of reference vertices must be a reference triangle, in order
    ++st.clip_runs;
    if (std::getenv("ORACLE_MRP_TRACE") != nullptr) {
        std::printf("  clip run: class %u packet %u start %03x: reference %zu, kicked %zu triangles in %zu packets\n", oc, k, run.start, ref.size(), got.size(), run.kicks.size());
        static int cshown = 0;
        auto pv = [](const char* w, const mrp::GsVertex& v) {
            std::printf("    %s stq %g %g %g rgba %d %d %d %d xyzf %d %d %u %x\n", w, v.s, v.t, v.q, v.rgba[0], v.rgba[1], v.rgba[2], v.rgba[3], static_cast<int32_t>(v.x),
                        static_cast<int32_t>(v.y), v.z, v.f);
        };
        for (size_t i = 0; i < ref.size() && i < got.size() && cshown < 3; ++i)
            if (!tri_eq(ref[i], got[i])) {
                ++cshown;
                std::printf("    first difference: triangle %zu\n", i);
                for (int c = 0; c < 3; ++c) pv("ref ", ref[i].v[c]);
                for (int c = 0; c < 3; ++c) pv("kick", got[i].v[c]);
                break;
            }
    }
    st.clip_tris += ref.size();
    size_t j = 0;
    for (const KTri& g : got) {
        size_t f = j;
        while (f < ref.size() && !tri_eq(ref[f], g)) ++f;
        if (f < ref.size()) {
            st.clip_culled += f - j;
            ++st.clip_matched;
            j = f + 1;
        } else {
            ++st.clip_new;
        }
    }
    st.clip_culled += ref.size() - std::min(ref.size(), j);
}

struct Stats {
    uint64_t packets = 0, slots = 0, slots_ok = 0, slots_stale = 0, pos_bad = 0, col_bad = 0, unmatched = 0, tinted = 0;
    std::set<uint32_t> classes;
    int shown = 0;
};

void check_frames() {
    const int frames = std::getenv("ORACLE_MRP_FRAMES") != nullptr ? std::atoi(std::getenv("ORACLE_MRP_FRAMES")) : 6;
    Stats sk;
    level_state();
    if (g_frame_failed) {
        skip_check("mrp_skin", 0x001ee858, "the level's first gameplay frame fails in the oracle");
        skip_check("mrp_vu1", 0x0010faa8, "the level's first gameplay frame fails in the oracle");
        return;
    }
    LevelClasses& lc = classes();
    for (int fi = 0; fi < frames; ++fi) {
        Frame f;
        run_frame(f);
        // the moby programs' starts in chain order = the records' packets in order
        std::vector<const Run*> runs;
        for (const Run& r : f.runs)
            if (r.start == 0x00a || r.start == 0x00e || r.start == 0x008 || r.start == 0x00c) runs.push_back(&r);
        size_t ri_ = 0;
        if (std::getenv("ORACLE_MRP_TRACE") != nullptr) {
            size_t np = 0;
            for (const Record& R : f.records) np += R.packets.size();
            std::printf("  frame %d: records %zu (packets %zu), program starts %zu (moby %zu)\n", fi, f.records.size(), np, f.runs.size(), runs.size());
            if (!f.records.empty() && !f.records[0].packets.empty()) std::printf("  first record packet class %u #%u, first run vif %08x\n", f.records[0].packets[0].first, f.records[0].packets[0].second, runs.empty() ? 0u : runs[0]->vif);
            std::printf("  classes %zu, by_vif %zu, by_vdata %zu, rec0 entry %08x %08x\n", lc.asset.size(), lc.by_vif.size(), lc.by_vdata.size(), ee_read<uint32_t>(f.records[0].addr + 0xc0), ee_read<uint32_t>(f.records[0].addr + 0xc4));
        }
        for (const Record& R : f.records) {
            mrp::SkinState st;
            for (int a = 0; a < 256; ++a)
                for (int l = 0; l < 4; ++l) std::memcpy(&st.vu0[static_cast<size_t>(a)][static_cast<size_t>(l)], &R.vu0_mem[static_cast<size_t>(a) * 16 + 4 * static_cast<size_t>(l)], 4);
            for (int c = 0; c < 512; ++c) {
                for (int l = 0; l < 3; ++l) std::memcpy(&st.cache_pos[static_cast<size_t>(c)][static_cast<size_t>(l)], &R.cache[static_cast<size_t>(c) * 16 + 2 * static_cast<size_t>(l)], 2);
                std::memcpy(st.cache_rgba[static_cast<size_t>(c)].data(), &R.cache[static_cast<size_t>(c) * 16 + 8], 4);
            }
            mrp::SkinInput in;
            in.joints = reinterpret_cast<const float(*)[16]>(R.joints.data());
            in.joint_count = 128;
            in.light = R.light;
            in.normal_table = reinterpret_cast<const float(*)[2]>(f.normals.data());
            for (size_t j = 0; j < R.packets.size(); ++j) {
                const auto [oc, k] = R.packets[j];
                // the packet's program start: the next one that REF'd this packet's VIF list
                size_t at = ri_;
                while (at < runs.size() && (lc.by_vif.count(runs[at]->vif) == 0 || lc.by_vif[runs[at]->vif] != std::pair<uint32_t, uint32_t>{oc, k})) ++at;
                if (at >= runs.size()) {
                    if (sk.shown++ < 10) std::printf("  mrp_skin: record %08x: no program start for class %u packet %u\n", R.addr, oc, k);
                    ++sk.unmatched;
                    continue;
                }
                const Run& run = *runs[at];
                ri_ = at + 1;
                auto it = lc.asset.find(oc);
                if (it == lc.asset.end() || k >= it->second->packets.size()) continue;
                const mrp::Packet& p = it->second->packets[k];
                std::vector<std::array<int16_t, 3>> pos;
                std::vector<std::array<uint8_t, 4>> rgba;
                mrp::skin_packet(p, in, st, pos, rgba);
                ++sk.packets;
                sk.classes.insert(oc);
                const uint32_t B = (run.start == 0x00a || run.start == 0x00e) ? (run.st.xtop & 0x3ff) : 356;
                for (uint32_t s = 0; s < p.n_slots; ++s) {
                    int32_t w[4], c[4];
                    std::memcpy(w, &run.st.mem[((B + s) & 0x3ff) * 16], 16);
                    std::memcpy(c, &run.st.mem[((B + 97 + s) & 0x3ff) * 16], 16);
                    ++sk.slots;
                    const bool stale = s >= p.vertices.size() && p.duplicate_source[s - p.vertices.size()][0] < 0;
                    if (stale) ++sk.slots_stale;
                    bool pok = true;
                    for (int l = 0; l < 3; ++l) pok = pok && std::abs(w[l] - pos[s][static_cast<size_t>(l)]) <= 1;
                    if (!pok) {
                        ++sk.pos_bad;
                        if (sk.shown++ < 12)
                            std::printf("  mrp_skin: class %u packet %u slot %u (%s): VU1 (%d %d %d) reference (%d %d %d)\n", oc, k, s + 1,
                                        s < p.vertices.size() ? (p.vertices[s].kind == mrp::VertexKind::Single ? "single" : p.vertices[s].kind == mrp::VertexKind::TwoWay ? "2-way" : "3-way") : "dup",
                                        w[0], w[1], w[2], pos[s][0], pos[s][1], pos[s][2]);
                    }
                    bool cok = true;
                    if (R.tint[j] >= 0) {  // (0x255870: the sub-mesh colour over every slot)
                        for (int l = 0; l < 4; ++l) cok = cok && c[l] == static_cast<int32_t>((R.tint[j] >> (8 * l)) & 0xff);
                        ++sk.tinted;
                    } else {
                        for (int l = 0; l < 4; ++l) cok = cok && std::abs(c[l] - rgba[s][static_cast<size_t>(l)]) <= 1;
                    }
                    if (!cok) {
                        ++sk.col_bad;
                        if (sk.shown++ < 12)
                            std::printf("  mrp_skin: class %u packet %u slot %u: colour VU1 (%d %d %d %d) reference (%d %d %d %d)%s\n", oc, k, s + 1, c[0], c[1], c[2], c[3],
                                        rgba[s][0], rgba[s][1], rgba[s][2], rgba[s][3], R.tint[j] >= 0 ? " (tinted)" : "");
                        if (sk.shown < 14 && s < p.vertices.size()) {
                            const auto& sv = p.vertices[s];
                            std::printf("    kind %d vertex colour %02x%02x%02x%02x normal %u %u, tint row %g %g %g %g, record packet %zu of %zu\n", static_cast<int>(sv.kind), sv.rgba[0], sv.rgba[1],
                                        sv.rgba[2], sv.rgba[3], sv.normal[0], sv.normal[1], R.light[7][0], R.light[7][1], R.light[7][2], R.light[7][3], j, R.packets.size());
                        }
                    }
                    if (pok) ++sk.slots_ok;
                }
                compare_vu1(vs, run, p, oc, k, lw(0x0015ee8c) >> 8);
            }
        }
    }
    std::printf("%-26s %s  %s (classes %zu, packets %llu, slots %llu: positions differ %llu, colours differ %llu, stale duplicates %llu)\n", "mrp_skin", "0x001ee858",
                sk.pos_bad == 0 && sk.col_bad == 0 && sk.unmatched == 0 ? "ok" : "FAIL", sk.classes.size(), static_cast<unsigned long long>(sk.packets), static_cast<unsigned long long>(sk.slots),
                static_cast<unsigned long long>(sk.pos_bad), static_cast<unsigned long long>(sk.col_bad), static_cast<unsigned long long>(sk.slots_stale));
    std::printf("  (sub-mesh colour slots %llu, packets without their program start %llu)\n", static_cast<unsigned long long>(sk.tinted), static_cast<unsigned long long>(sk.unmatched));
    if (sk.pos_bad != 0 || sk.col_bad != 0 || sk.unmatched != 0) ++g_fail;
    const bool vok = vs.ref_missing == 0 && vs.extra == 0 && vs.tex_bad == 0 && vs.untraced == 0 && vs.runs > 0;
    std::printf("%-26s %s  %s (classes %zu, program runs %llu: no-clip triangles %llu, equal %llu, differing / missing %llu, extra kicked %llu, texture state differs %llu)\n", "mrp_vu1",
                "0x0010faa8", vok ? "ok" : "FAIL", vs.classes.size(), static_cast<unsigned long long>(vs.runs), static_cast<unsigned long long>(vs.tris),
                static_cast<unsigned long long>(vs.tris_ok), static_cast<unsigned long long>(vs.ref_missing), static_cast<unsigned long long>(vs.extra),
                static_cast<unsigned long long>(vs.tex_bad));
    std::printf("  (packets without a kick program: %llu)\n", static_cast<unsigned long long>(vs.untraced));
    std::printf("  (clipping program: %llu runs, %llu reference triangles: %llu kicked unchanged, %llu culled or clipped, %llu kicked triangles with clip-made vertices)\n",
                static_cast<unsigned long long>(vs.clip_runs), static_cast<unsigned long long>(vs.clip_tris), static_cast<unsigned long long>(vs.clip_matched),
                static_cast<unsigned long long>(vs.clip_culled), static_cast<unsigned long long>(vs.clip_new));
    if (!vok) ++g_fail;
}

}  // namespace

// ---------------------------------------------------------------- mrp_scene_asset

// The MRP's two inputs together: task 0318's per-frame scene state (src/game/render/mrp_scene.h:
// the pose the skinning read, the record's light rows, the parts' UNPACKs, from the unmodified pass
// 0x250038) and this task's assets. Per part of every moby drawn, the asset's packet skinned and lit
// by mrp::skin_packet from the scene's joints / record rows, against the scene's UNPACKs of the
// positions and colours (what VU1 receives).
void check_scene() {
    level_state();
    if (g_frame_failed) {
        skip_check("mrp_scene_asset", 0x00250038, "the level's first gameplay frame fails in the oracle");
        return;
    }
    // (the scene's parts come from the draw loop's notes by default, without the unpacks this check reads: mrp_scene.cpp,
    // RAC1_PERF_MOBY_PARTS; with it 0, or with RAC1_MRP_CHECK, the chains are decoded as before)
    if (const char* pm = std::getenv("RAC1_PERF_MOBY_PARTS"); (pm == nullptr || std::atoi(pm) != 0) && std::getenv("RAC1_MRP_CHECK") == nullptr) {
        skip_check("mrp_scene_asset", 0x00250038, "run with RAC1_PERF_MOBY_PARTS=0: the scene's parts decoded from the chain, with their unpacks");
        return;
    }
    LevelClasses& lc = classes();
    const int frames = std::getenv("ORACLE_MRP_FRAMES") != nullptr ? std::atoi(std::getenv("ORACLE_MRP_FRAMES")) : 6;
    static bool installed = false;
    if (!installed) {
        mrp_scene_install();
        installed = true;
    }
    uint64_t parts = 0, slots = 0, pos_bad = 0, col_bad = 0, unknown = 0, tinted = 0;
    std::set<uint32_t> cls_seen;
    int shown = 0;
    for (int fi = 0; fi < frames; ++fi) {
        place_mobys();
        vu0m::regs() = VUState{};
        vu0m::regs().vf[0].f[3] = 1.0f;
        vu0::vif0_send_chain(0x0010e4c0, true);
        sub_002a07f8();
        sub_002a0858();
        w32(OV(0x0016a018), 1);
        w32(OV(0x0016a014), 1);
        mrp_scene_begin_frame();
        moby_pass();
        mrp_scene_end_frame();
        const MrpFrame& f = mrp_scene();
        std::vector<float> normals(0x200);
        for (uint32_t i = 0; i < 0x200; ++i) normals[i] = ee_read<float>(OV(0x00166080) + 4 * i);
        for (const MrpMoby& m : f.mobys) {
            if (m.shadow_mesh || m.deferred) continue;
            const uint32_t oc = static_cast<uint32_t>(static_cast<uint16_t>(m.class_id));
            // (the class by its header address: a lent moby's +0xa6 is still its own class's)
            uint32_t found = 0xffffffffu;
            for (const auto& [o, h] : lc.header)
                if (h == m.class_ptr) found = o;
            (void)oc;
            auto it = lc.asset.find(found);
            if (it == lc.asset.end()) {
                unknown += m.part_count;
                continue;
            }
            const mrp::MobyClass& a = *it->second;
            mrp::SkinState st;
            mrp::SkinInput in;
            std::vector<float> joints(static_cast<size_t>(m.joint_count) * 16);
            for (uint32_t j = 0; j < m.joint_count; ++j)
                for (int r = 0; r < 4; ++r) {
                    const MrpVec4& v = f.joints[m.joint_first + j].r[r];
                    joints[j * 16 + static_cast<uint32_t>(r) * 4 + 0] = v.x;
                    joints[j * 16 + static_cast<uint32_t>(r) * 4 + 1] = v.y;
                    joints[j * 16 + static_cast<uint32_t>(r) * 4 + 2] = v.z;
                    joints[j * 16 + static_cast<uint32_t>(r) * 4 + 3] = v.w;
                }
            in.joints = reinterpret_cast<const float(*)[16]>(joints.data());
            in.joint_count = m.joint_count;
            for (uint32_t q = 0; q < 8; ++q)
                for (uint32_t l = 0; l < 4; ++l) std::memcpy(&in.light[q][l], &f.records[m.record_first + 0x40 + q * 0x10 + l * 4], 4);
            in.normal_table = reinterpret_cast<const float(*)[2]>(normals.data());
            for (uint32_t k = 0; k < m.part_count; ++k) {
                const uint32_t pk = m.first_packet + k;
                if (pk >= a.packets.size()) continue;
                const mrp::Packet& p = a.packets[pk];
                std::vector<std::array<int16_t, 3>> pos;
                std::vector<std::array<uint8_t, 4>> rgba;
                mrp::skin_packet(p, in, st, pos, rgba);
                const MrpPart& part = f.parts[m.part_first + k];
                const MrpUnpack* up = nullptr;
                const MrpUnpack* uc = nullptr;
                for (uint32_t u = 0; u < part.unpack_count; ++u) {
                    const MrpUnpack& un = f.unpacks[part.unpack_first + u];
                    if (un.cmd == 0x69 && un.dest == 0) up = &un;
                    if (un.cmd == 0x6e && un.dest == 97) uc = &un;
                }
                if (up == nullptr || uc == nullptr) {
                    ++unknown;
                    continue;
                }
                ++parts;
                cls_seen.insert(found);
                const bool tint = (m.flags & 0x10) != 0 && p.index_in_lod >= a.tint_first[p.lod];
                for (uint32_t s = 0; s < p.n_slots && s < up->num; ++s) {
                    ++slots;
                    bool pok = true;
                    for (uint32_t l = 0; l < 3; ++l) pok = pok && std::abs(ee_read<int16_t>(up->data + s * 6 + l * 2) - pos[s][l]) <= 1;
                    bool cok = true;
                    for (uint32_t l = 0; l < 4; ++l) {
                        const int32_t c = ee_read<uint8_t>(uc->data + s * 4 + l);
                        // (0x256ab0: the sub-mesh colour is moby +0x90 with the alpha the record's +0xbc byte)
                        const int32_t want = tint ? static_cast<int32_t>(l == 3 ? f.records[m.record_first + 0xbc] : ee_read<uint8_t>(m.moby + 0x90 + l)) : rgba[s][l];
                        cok = cok && (tint ? c == want : std::abs(c - want) <= 1);
                    }
                    tinted += tint ? 1 : 0;
                    if (!pok) ++pos_bad;
                    if (!cok) ++col_bad;
                    if ((!pok || !cok) && shown++ < 8) std::printf("  mrp_scene_asset: class %u packet %u slot %u differs\n", found, pk, s + 1);
                }
            }
        }
    }
    const bool ok = pos_bad == 0 && col_bad == 0 && parts > 0;
    std::printf("%-26s %s  %s (classes %zu, parts %llu, slots %llu: positions differ %llu, colours differ %llu; sub-mesh colour slots %llu, parts without an asset %llu)\n",
                "mrp_scene_asset", "0x00250038", ok ? "ok" : "FAIL", cls_seen.size(), static_cast<unsigned long long>(parts), static_cast<unsigned long long>(slots),
                static_cast<unsigned long long>(pos_bad), static_cast<unsigned long long>(col_bad), static_cast<unsigned long long>(tinted), static_cast<unsigned long long>(unknown));
    if (!ok) ++g_fail;
}

// ---------------------------------------------------------------- mrp_capture

// The VU1 start states captured in the running game (RAC1_VU1_CAPTURE: assets/dumps/oracle/vu1_moby/
// <dir>, Veldin's intro and gameplay, Kerwan): each moby program start's packet found among the
// assets by its index list and UVs in VU1 memory, the ported program run on the state, its GS
// triangles against the asset's kick program evaluated on the state's slots and constants.
void check_captures() {
    const char* d0 = std::getenv("ORACLE_VU1_DIR");
    const std::string dir = d0 != nullptr ? std::string(d0) : dump_path("vu1_moby");
    const char* m0 = std::getenv("ORACLE_MRP_DIR");
    const std::string root = m0 != nullptr ? m0 : "assets/generated/mrp/moby";
    // the candidate packets: the levels the captures come from, the armor
    std::vector<std::unique_ptr<mrp::MobyClass>> all;
    for (const char* sub : {"level0", "level3", "armor"}) {
        const std::string ld = root + "/" + sub;
        if (!std::filesystem::is_directory(ld)) continue;
        for (const auto& de : std::filesystem::directory_iterator(ld)) {
            if (de.path().extension() != ".mrpm") continue;
            auto c = std::make_unique<mrp::MobyClass>();
            if (mrp::read_class(de.path().string(), *c)) all.push_back(std::move(c));
        }
    }
    std::multimap<uint64_t, std::pair<const mrp::MobyClass*, size_t>> by_key;
    auto key_of = [](const int8_t* h, const int8_t* ix) {
        uint64_t k = 0;
        for (int b = 0; b < 4; ++b) k = (k << 8) | static_cast<uint8_t>(h[b]);
        for (int b = 0; b < 4; ++b) k = (k << 8) | static_cast<uint8_t>(ix[b]);
        return k;
    };
    for (const auto& c : all)
        for (size_t k = 0; k < c->packets.size(); ++k) {
            const mrp::Packet& p = c->packets[k];
            if (p.lod == 2 || p.indices.size() < 4) continue;
            by_key.insert({key_of(p.index_header, p.indices.data()), {c.get(), k}});
        }
    std::vector<std::string> files;
    if (std::filesystem::is_directory(dir))
        for (const auto& de : std::filesystem::recursive_directory_iterator(dir))
            if (de.path().extension() == ".bin") files.push_back(de.path().string());
    std::sort(files.begin(), files.end());
    if (files.empty()) {  // no captures recorded on this machine: skipped, not failed
        std::printf("%-26s %s  skipped (no captured states in %s: run rac1-native with RAC1_VU1_CAPTURE=<dir>)\n", "mrp_capture", "0x0010faa8", dir.c_str());
        ++g_skipped;
        return;
    }
    VStats cs;
    int states = 0, found = 0, unknown = 0;
    auto micro = std::make_unique<std::array<uint64_t, 2048>>();
    for (const std::string& path : files) {
        FILE* fp = std::fopen(path.c_str(), "rb");
        if (fp == nullptr) continue;
        uint32_t hdr[4];
        auto st = std::make_unique<VUState>();
        float fr[4];
        uint32_t fl[4];
        const bool rd = std::fread(hdr, 4, 4, fp) == 4 && std::fread(st->vf.data(), sizeof(VUVector), 32, fp) == 32 && std::fread(st->vi.data(), 2, 16, fp) == 16 &&
                        std::fread(&st->acc, sizeof(VUVector), 1, fp) == 1 && std::fread(fr, 4, 4, fp) == 4 && std::fread(fl, 4, 4, fp) == 4 &&
                        std::fread(st->mem.data(), 1, st->mem.size(), fp) == st->mem.size() && std::fread(micro->data(), 8, 2048, fp) == 2048;
        std::fclose(fp);
        if (!rd || hdr[0] != 0x31555676u) continue;
        if (vu1::code_hash(micro->data(), 0x508) != 0x1585563a86921802ull) continue;
        const uint32_t start = hdr[1];
        if (start != 0x00a && start != 0x00e && start != 0x008 && start != 0x00c) continue;
        st->q = fr[0], st->i = fr[1], st->r = fr[2], st->p = fr[3];
        st->macflag = fl[0], st->statusflag = fl[1], st->clipflag = fl[2];
        st->xtop = hdr[2];
        ++states;
        const uint32_t B = (start == 0x00a || start == 0x00e) ? (st->xtop & 0x3ff) : 356;
        auto byte_at = [&](uint32_t qw, int lane) {
            int32_t w;
            std::memcpy(&w, &st->mem[((qw & 0x3ff) << 4) + 4 * static_cast<uint32_t>(lane)], 4);
            return static_cast<int8_t>(w);
        };
        int8_t h[4], ix[4];
        for (int l = 0; l < 4; ++l) {
            h[l] = byte_at(B + 301, l);
            ix[l] = byte_at(B + 302, l);
        }
        const mrp::MobyClass* hit = nullptr;
        size_t hk = 0;
        auto range = by_key.equal_range(key_of(h, ix));
        for (auto it = range.first; it != range.second && hit == nullptr; ++it) {
            const mrp::Packet& p = it->second.first->packets[it->second.second];
            bool same = true;
            for (size_t j = 0; j < p.indices.size() && same; ++j) same = byte_at(B + 302 + static_cast<uint32_t>(j / 4), static_cast<int>(j % 4)) == p.indices[j];
            for (size_t s = 0; s < p.uv.size() && same; ++s) {
                int32_t u0, u1;
                std::memcpy(&u0, &st->mem[((B + 194 + s) & 0x3ff) << 4], 4);
                std::memcpy(&u1, &st->mem[(((B + 194 + s) & 0x3ff) << 4) + 4], 4);
                same = u0 == p.uv[s][0] && u1 == p.uv[s][1];
            }
            if (same) {
                hit = it->second.first;
                hk = it->second.second;
            }
        }
        if (hit == nullptr) {
            ++unknown;
            continue;
        }
        ++found;
        Run run;
        run.start = start;
        run.st = *st;
        std::vector<std::vector<uint8_t>>& out = run.kicks;
        st->xgkick_hook = [&](uint32_t addr) {
            const uint32_t qwc = vu1::gif_packet_qwc(*st, addr);
            std::vector<uint8_t> b(static_cast<size_t>(qwc) * 16);
            for (uint32_t q = 0; q < qwc; ++q) std::memcpy(&b[q * 16], &st->mem[((addr + q) & 0x3ff) << 4], 16);
            out.push_back(std::move(b));
        };
        VuMicroRun mr;
        mr.start(*st, nullptr, 2048, start, &moby_vu1_step);
        mr.limit_issue(2000000);
        while (mr.pipe().issued < 2000000 && mr.step()) {
        }
        mr.finish();
        st->xgkick_hook = nullptr;
        compare_vu1(cs, run, hit->packets[hk], static_cast<uint32_t>(hit->o_class), static_cast<uint32_t>(hk), INT32_MIN);
    }
    const bool ok = cs.ref_missing == 0 && cs.extra == 0 && cs.untraced == 0 && found > 0 && unknown == 0;
    std::printf("%-26s %s  %s (%d captured program starts in %s: %d packets found (%zu classes), %d unknown; no-clip triangles %llu, equal %llu, differing %llu, extra %llu)\n",
                "mrp_capture", "0x0010faa8", ok ? "ok" : "FAIL", states, dir.c_str(), found, cs.classes.size(), unknown, static_cast<unsigned long long>(cs.tris),
                static_cast<unsigned long long>(cs.tris_ok), static_cast<unsigned long long>(cs.ref_missing), static_cast<unsigned long long>(cs.extra));
    std::printf("  (clipping program: %llu runs, %llu reference triangles: %llu kicked unchanged, %llu culled or clipped, %llu kicked triangles with clip-made vertices)\n",
                static_cast<unsigned long long>(cs.clip_runs), static_cast<unsigned long long>(cs.clip_tris), static_cast<unsigned long long>(cs.clip_matched),
                static_cast<unsigned long long>(cs.clip_culled), static_cast<unsigned long long>(cs.clip_new));
    if (!ok) ++g_fail;
}

void batch_93() {
    if (g_only_batch != 93) return;
    g_batch = 93;
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    const char* only = std::getenv("ORACLE_ONLY");
    if (only == nullptr || std::string(only) == "mrp_static") check_static();
    if (only == nullptr || std::string(only) == "mrp_skin" || std::string(only) == "mrp_vu1") {
        static bool reg = false;
        if (!reg) {
            moby_vu0_register();
            moby_vu1_register();
            reg = true;
        }
        check_frames();
    }
    if (only == nullptr || std::string(only) == "mrp_scene_asset") {
        moby_vu0_register();
        moby_vu1_register();
        check_scene();
    }
    if (only == nullptr || std::string(only) == "mrp_capture") check_captures();
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
