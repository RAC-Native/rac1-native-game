// orig: (host) The MRP moby renderer (see mrp_moby_renderer.h).
#include <game/render/mrp_moby_renderer.h>
#include <profiler/host_profiler.h>
#include <jobs/job_pool.h>

#include <formats/moby_mrp.h>
#include <game/boot/front_end.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_scene.h>
#include <game/render/mrp_shadows.h>
#include <game/render/mrp_world_assets.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/gs/gs_draw.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/kernel/ee_memory.h>
#include "io/host_endian.h"
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace rac1 {

int g_mrp_moby_projection = 0;
bool g_mrp_moby_keep_scissor = false;
int g_mrp_moby_mip = 0;
int g_mrp_moby_fog = 1;
// The debug menu's free hoverboard (src/_devtools/dev_hoverboard.cpp; declared there, not in a header: the generated code is
// fingerprinted with the headers of src/game, and a debug tool must not ask for ./setup.sh again): another level's class
// drawn on the hero's feet. `g_mrp_guest_class`: level * 10000 + o_class to load beside the level's own (-1 none; loaded at
// the next frame, with its textures added to the level's); `g_mrp_guest_on`: drawn this frame; `g_mrp_guest_lift`: its
// origin above the hero's along his up axis.
std::atomic<int> g_mrp_guest_class{-1};
std::atomic<bool> g_mrp_guest_on{false};
std::atomic<float> g_mrp_guest_lift{0.0f};

namespace {

// ---- GL 4.x entry points (the loader the runtime uses stops at 3.3; the context is 4.6) -------

using PfnMultiDrawElementsIndirect = void(APIENTRYP)(GLenum, GLenum, const void*, GLsizei, GLsizei);
using PfnDrawArraysInstancedBaseInstance = void(APIENTRYP)(GLenum, GLint, GLsizei, GLsizei, GLuint);
PfnMultiDrawElementsIndirect p_multi_draw_elements_indirect = nullptr;
PfnDrawArraysInstancedBaseInstance p_draw_arrays_instanced_base_instance = nullptr;
constexpr GLenum kGlShaderStorageBuffer = 0x90D2, kGlDrawIndirectBuffer = 0x8F3F, kGlMaxAnisotropy = 0x84FE;

const bool g_env_diff = std::getenv("RAC1_MRP_DIFF") != nullptr;
const bool g_env_check = std::getenv("RAC1_MRP_CHECK") != nullptr;
const bool g_env_prof = std::getenv("RAC1_FRAME_PROF") != nullptr;
// (RAC1_FRAME_PROF=2, no glFinish inside the stages: the host times them with GPU queries)
const bool g_env_prof_finish = g_env_prof && std::atoi(std::getenv("RAC1_FRAME_PROF")) <= 1;
const bool g_env_debug = std::getenv("RAC1_MRP_DEBUG") != nullptr;

double now_ms() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// ---- the GPU data of a class -----------------------------------------------------------------

// One GPU vertex: a VU1 slot of a packet as the kick program sends it (its position, colour, UV are
// the slot's: the asset's kick ops never mix slots but for STQ's second Q factor, kept as q2).
// The slot's skinning record: bind position, normal angles, vertex colour, the VU0 matrix it is
// skinned with as <= 3 weighted joints (n 1: the joint's matrix itself; 2 / 3: the VU0 blend
// M[j0] w0 + M[j1] w1 (+ M[j2] w2), in that order).
struct GpuVertex {
    int16_t pos[3];
    uint16_t normal;  // the angle bytes n0 | n1 << 8
    uint8_t joint[3];
    uint8_t n;
    uint8_t weight[3];
    uint8_t flags;  // 1: STQ x the Q of the q2 slot
    uint8_t rgba[4];
    int16_t uv[2];
    int16_t q2pos[3];
    int16_t pad0;
    uint8_t q2joint[3];
    uint8_t q2n;
    uint8_t q2weight[3];
    uint8_t pad1;
};
static_assert(sizeof(GpuVertex) == 40);

// a slot's skinning record resolved through the VU0 matrix memory
struct SlotRec {
    int16_t pos[3] = {0, 0, 0};
    uint16_t normal = 0;
    uint8_t joint[3] = {0, 0, 0};
    uint8_t n = 0;  // 0: unresolved (a zero matrix)
    uint8_t weight[3] = {0, 0, 0};
    uint8_t rgba[4] = {0, 0, 0, 0};
};

// the texture of a prim, as a draw record word
struct PrimInfo {
    int bucket = -1;      // texture array (-1: the white 8 x 8 texture, or none found)
    uint32_t layer = 0;
    uint32_t word = 0;    // bit 0 white, 1..2 TFX, 3 TCC, 4..6 MXL, 7 LCM, 8..9 L, 10..21 K (raw 12 bits), 22..24 mips - 1
    uint32_t sampler = 0; // bit 0 MMAG linear, 1 MMIN linear, 2 wrap s repeat, 3 wrap t repeat
    int game_texture = -1;  // the texture's index in the game's moby texture tables (0x199a80 / 0x199e80)
    int local = -1;         // the class's local texture (0..15; the emissive surfaces' key)
    uint64_t tex1 = 0, clamp = 0, tex0 = 0;  // the registers it came from (the checks)
};

struct Run {
    uint32_t first_index = 0, count = 0;  // into the level's index buffer (absolute vertex indices)
    int prim = -1;                        // the packet's texture prim (-1: the one in force before the packet)
};

struct PacketGpu {
    uint32_t first_vertex = 0, vertex_count = 0;
    std::vector<Run> runs;
    std::vector<PrimInfo> prims;
    std::vector<std::array<uint8_t, 2>> vslots;  // per vertex: (slot, q2 slot) (the shader check)
    bool tint_part = false;  // the sub-mesh colour applies (packet index in LOD >= class +0xa / +0xb)
    int last_prim = -1;      // the packet's last texture switch (the GS's TEX0 after it: the next packets' until they switch)
};

struct ClassGpu {
    std::unique_ptr<mrp::MobyClass> asset;
    std::vector<PacketGpu> packets;  // as the class's packet table (lod 2: empty)
    bool from_memory_prims = false;
    uint32_t vdata_ptr = 0;  // the class's first packet's vertex data in EE memory when built
};

// ---- level state -------------------------------------------------------------------------------

struct Bucket {
    uint32_t w = 0, h = 0, levels = 0;
    GLuint tex = 0;
    vk::GpuImage vk_tex{};
    std::vector<int> textures;  // the level texture of each layer
};

struct Level {
    int id = -1;
    bool loaded = false;
    bool gl_loaded = false;
    bool vk_loaded = false;
    bool missing = false;  // (the level's assets not there: not looked for again every frame)
    std::vector<mrp::Texture> textures;
    std::vector<int> tex_bucket, tex_layer;
    std::vector<Bucket> buckets;
    std::vector<std::unique_ptr<mrp::MobyClass>> armor;  // armor/<k>_<s>.mrpm (the hero's classes, matched by vertex data)
    std::vector<int> armor_k, armor_s;                   // their armor file k, class s
    size_t level_textures = 0;  // the level's moby textures (textures.mrpt); the armors' after them, 2 per file
    int armor_base = 0;
    int armor_file = -1;        // the armor file whose classes the hero has (the last matched)
    int build_armor_file = -1;  // the class being built's
    std::map<int, std::unique_ptr<mrp::MobyClass>> files;  // o_class -> asset (not yet on the GPU)
    // the free hoverboard's class (g_mrp_guest_class): its asset, its textures renumbered into this level's (after the
    // armors'), its GPU data built at its first draw (again after the level's vertex data is dropped)
    std::unique_ptr<mrp::MobyClass> guest;
    std::unique_ptr<ClassGpu> guest_gpu;
    int guest_key = -1;  // the g_mrp_guest_class loaded (or tried)
    // the class header in EE memory and the moby's class id (+0xa6) -> GPU data (the weapons share one header, the class
    // table naming it under each weapon's id: the header alone named the last of them)
    std::unordered_map<uint64_t, std::unique_ptr<ClassGpu>> by_ptr;
    std::unordered_map<uint64_t, int> miss;  // (header, id) with no asset (counted once)
    std::vector<GpuVertex> verts;
    std::vector<uint32_t> indices;
    bool dirty = false;
    bool vk_dirty = false;
    uint64_t table_hash = 0;
    GLuint vao = 0, vbo = 0, ibo = 0;
    size_t vbo_cap = 0, ibo_cap = 0;
    vk::GpuBuffer vk_vbo{};
    vk::GpuBuffer vk_ibo{};
};
Level g_level;

// ---- statistics (reported at the end) ----
struct Stats {
    uint64_t classes = 0, packets = 0, vertices = 0, triangles = 0, mixed_ops = 0, startq_ops = 0, q2_vertices = 0;
    uint64_t unresolved_matrix = 0, nested_blend = 0, misaligned = 0, prev_texture_runs = 0;
    uint64_t tri_check_packets = 0, tri_check_bad = 0;  // the load's triangles against mrp::kick_triangles
    uint64_t prims_asset = 0, prims_memory = 0, prims_white = 0, prims_missing = 0, prims_mismatch = 0, mxl_over_mips = 0;
    uint64_t frames = 0, mobys = 0, parts = 0, draws = 0, gl_calls = 0, no_asset_mobys = 0, shadow_parts = 0, part_mismatch = 0, cc_mobys = 0, cc_bad = 0;
    // shader check (transform feedback against skin_packet / vu1_vertex)
    uint64_t chk_parts = 0, chk_vertices = 0, chk_pos_exact = 0, chk_pos_1 = 0, chk_pos_bad = 0, chk_col_exact = 0, chk_col_1 = 0, chk_col_bad = 0;
    uint64_t chk_stq_bad = 0, chk_xy_exact = 0, chk_xy_1 = 0, chk_xy_bad = 0, chk_z_bad = 0, chk_tint = 0, chk_fog = 0, chk_fog_bad = 0, chk_fog_clip = 0;
    uint64_t chk_chain_slots = 0, chk_chain_pos = 0, chk_chain_col = 0, chk_chain_missing = 0;  // the reference against the frame's chain
    // the faithful pass's GS triangles against the kick program (RAC1_MRP_DIFF + RAC1_MRP_CHECK)
    uint64_t gs_frames = 0, gs_frames_unmatched = 0, gs_parts = 0, gs_clip_parts = 0, gs_parts_count_differ = 0, gs_parts_differ = 0, gs_tris_ref = 0, gs_tris_faithful = 0,
             gs_tris_differ = 0;
    uint64_t kick_tex = 0, kick_tex_bad = 0, kick_tex_stale = 0;  // the same against the faithful triangles' own texture images (at their kick)
    uint64_t kick_tex_level[4] = {};
    uint64_t gs_fogcol_tris = 0, gs_fogcol_bad = 0;  // the faithful triangles' FOGCOL against the register the MRP reads at its draw
    uint64_t gs_fog_verts = 0, gs_fog_bad = 0;  // the faithful GS triangles' F bytes (XYZF2) against the kick program's
    uint64_t clip_tris = 0, clip_tris_matched = 0, clip_tris_differ = 0;  // the clipping program's triangles kept whole
    uint64_t state_tris = 0, state_differ = 0;  // the faithful triangles' GS state (PRIM, TEST, ALPHA) against the MRP's
    uint64_t reg_tris = 0, reg_differ = 0;      // their texture registers (TEX1, CLAMP, TEX0 but the addresses)
    // image diff
    uint64_t diff_frames = 0, px_a = 0, px_b = 0, px_both = 0, px_only_a = 0, px_only_b = 0, px_diff8 = 0, px_sum_abs = 0;
    // world-mode transform sanity (the rows' world matrix against the moby's own transform)
    double world_err_max = 0;
    // profile
    double prof_build_ms = 0, prof_draw_ms = 0;
    int prof_frames = 0;
};
Stats g_st;

std::string asset_root() {
    const char* r = std::getenv("RAC1_MRP_DIR");
    return r != nullptr ? r : "assets/generated/mrp/moby";
}

// the assets' level: the level 0x15ed84 names, or the boot module's title level while the boot runs
// (while the loading screen shows, its own scene: the flight's assets)
int moby_level_id() { return level_exit_loading_screen() ? mrpw::kFlightLevel : boot_active() ? mrpw::kTitleLevel : lw(0x0015ed84); }

// ---- class conversion ----------------------------------------------------------------------------

// orig: (host) one VU0 matrix of the VU0's data memory as the skinning leaves it: a joint (n 1) or
// a blend of joints (n 2 / 3), stored at a quadword address, row r at address + r
struct Vu0Mat {
    uint8_t n = 0, joint[3] = {}, weight[3] = {};
    int origin = -1;  // the store (rows of one store belong together)
    int8_t row = -1;
};

// orig: (host) the slots of every packet of one LOD resolved: each record's matrix as the VU0
// program reads it (the stores before the reads, the blends), the duplicates through the vertex cache
void resolve_lod(const mrp::MobyClass& c, int lod, std::vector<std::vector<SlotRec>>& out) {
    std::array<Vu0Mat, 256> mem{};
    int store_id = 0;
    std::array<SlotRec, 512> cache{};
    auto store = [&](uint8_t at, const Vu0Mat& m) {
        ++store_id;
        for (int r = 0; r < 4; ++r) {
            Vu0Mat& x = mem[static_cast<uint8_t>(at + r)];
            x = m;
            x.origin = store_id;
            x.row = static_cast<int8_t>(r);
        }
    };
    auto read = [&](uint8_t at, Vu0Mat& m) {
        const Vu0Mat& x = mem[at];
        bool ok = x.n != 0 && x.row == 0;
        for (int r = 1; r < 4 && ok; ++r) ok = mem[static_cast<uint8_t>(at + r)].origin == x.origin && mem[static_cast<uint8_t>(at + r)].row == r;
        if (!ok) {
            if (x.n == 0) ++g_st.unresolved_matrix;
            else ++g_st.misaligned;
            m = Vu0Mat{};
            return false;
        }
        m = x;
        return true;
    };
    for (size_t pi = 0; pi < c.packets.size(); ++pi) {
        const mrp::Packet& p = c.packets[pi];
        if (p.lod != lod) continue;
        std::vector<SlotRec>& slots = out[pi];
        slots.assign(p.n_slots, SlotRec{});
        for (const auto& mt : p.matrix_transfers) {
            Vu0Mat j;
            j.n = 1;
            j.joint[0] = mt[0];
            store(mt[1], j);
        }
        for (size_t v = 0; v < p.vertices.size(); ++v) {
            const mrp::SkinVertex& sv = p.vertices[v];
            SlotRec r;
            for (int l = 0; l < 3; ++l) r.pos[l] = sv.pos[l];
            r.normal = static_cast<uint16_t>(sv.normal[0] | (sv.normal[1] << 8));
            std::memcpy(r.rgba, sv.rgba, 4);
            if (sv.kind == mrp::VertexKind::Single || sv.kind == mrp::VertexKind::TwoWay) {
                Vu0Mat j;
                j.n = 1;
                j.joint[0] = sv.joint;
                store(sv.store, j);
            }
            if (sv.kind == mrp::VertexKind::Single) {
                Vu0Mat m;
                if (read(sv.src[0], m)) {
                    r.n = m.n;
                    std::memcpy(r.joint, m.joint, 3);
                    std::memcpy(r.weight, m.weight, 3);
                }
            } else {
                const int nw = sv.kind == mrp::VertexKind::TwoWay ? 2 : 3;
                Vu0Mat blend;
                blend.n = static_cast<uint8_t>(nw);
                bool ok = true;
                for (int k = 0; k < nw; ++k) {
                    Vu0Mat m;
                    if (!read(sv.src[k], m)) {
                        ok = false;
                        continue;
                    }
                    if (m.n != 1) {  // a blend of a blend: not met in the shipped data (counted)
                        ++g_st.nested_blend;
                        ok = false;
                    }
                    blend.joint[k] = m.joint[0];
                    blend.weight[k] = sv.weight[k];
                }
                if (!ok) blend.n = 0;
                r.n = blend.n;
                std::memcpy(r.joint, blend.joint, 3);
                std::memcpy(r.weight, blend.weight, 3);
                if (blend.n != 0) store(sv.blend_store, blend);
                else {  // an unresolved blend stored as such: what reads it is unresolved too
                    Vu0Mat z;
                    store(sv.blend_store, z);
                }
            }
            if (v < slots.size()) slots[v] = r;
            cache[sv.cache_index & 0x1ff] = r;
        }
        for (size_t d = 0; d < p.duplicates.size(); ++d) {
            const size_t s = p.vertices.size() + d;
            if (s >= slots.size()) break;
            slots[s] = cache[p.duplicates[d] & 0x1ff];
        }
    }
}

// orig: (host) a texture prim's GS fields and its level texture (the asset's registers, or the class's
// prims as the level load rebuilt them in EE memory when the asset could not build them)
PrimInfo prim_info(const mrp::TexturePrim& t, uint32_t mem_prim, bool& from_memory) {
    PrimInfo pi;
    pi.local = t.local_texture;
    uint64_t tex1 = 0, clamp = 0, tex0 = 0;
    int texture = -1;
    bool known = t.gs_known;
    if (known) {
        tex1 = t.gs[0];
        clamp = t.gs[1];
        tex0 = t.gs[2];
        texture = t.texture;
        pi.game_texture = texture;
        ++g_st.prims_asset;
    } else if (mem_prim != 0) {  // the class in memory: the prim's 4 qwords TEX1, CLAMP, TEX0, MIPTBP1 (0x2400f0's order)
        auto q64 = [mem_prim](uint32_t q) { return static_cast<uint64_t>(lwu(mem_prim + 16 * q)) | (static_cast<uint64_t>(lwu(mem_prim + 16 * q + 4)) << 32); };
        tex1 = q64(0);
        clamp = q64(1);
        tex0 = q64(2);
        texture = t.texture == -1 ? -1 : static_cast<int>((clamp >> 24) & 0xff);  // (0x2400f0: CLAMP = c | d << 2 | ti << 24)
        if (t.texture < -1) texture = -2;
        pi.game_texture = texture;
        // a texture past the level's: the armor's two (0x23fa00 appends them to the level's textures: 0x199a80[n], [n + 1])
        // (the armor file: the one whose first class - its own mesh, unique per file - the hero's classes matched by
        // their vertex data; the second class and its texture are the same in the files)
        const int level_n = static_cast<int>(g_level.level_textures);
        if (texture >= level_n && texture < level_n + 2 && g_level.build_armor_file >= 0) texture = g_level.armor_base + g_level.build_armor_file * 2 + (texture - level_n);
        from_memory = true;
        known = true;
        ++g_st.prims_memory;
    }
    if (!known) {
        ++g_st.prims_missing;
        pi.word = 1;  // white
        return pi;
    }
    pi.tex1 = tex1;
    pi.clamp = clamp;
    pi.tex0 = tex0;
    const uint32_t tfx = gs::field(tex0, 35, 2), tcc = gs::field(tex0, 34, 1);
    const uint32_t mxl = std::min(gs::field(tex1, 2, 3), 3u), lcm = gs::field(tex1, 0, 1), l = gs::field(tex1, 19, 2), k = gs::field(tex1, 32, 12);
    pi.word = (tfx << 1) | (tcc << 3) | (mxl << 4) | (lcm << 7) | (l << 8) | (k << 10);
    pi.sampler = (gs::field(tex1, 5, 1) != 0 ? 1u : 0u) | (gs::field(tex1, 6, 3) >= 4 ? 2u : 0u) | (gs::field(clamp, 0, 2) == 0 ? 4u : 0u) |
                 (gs::field(clamp, 2, 2) == 0 ? 8u : 0u);
    if (texture == -1) {  // TEX0 0x...7ffb: the 8 x 8 texture of 0x80808080 the boot uploads at block 0x3ffb (boot_start.cpp)
        pi.word |= 1;
        ++g_st.prims_white;
        return pi;
    }
    if (texture < 0 || static_cast<size_t>(texture) >= g_level.tex_bucket.size() || g_level.tex_bucket[static_cast<size_t>(texture)] < 0) {
        ++g_st.prims_missing;
        if (g_env_debug) {
            std::printf("[mrp] prim without a texture: asset texture %d local %d known %d, memory prim %08x tex1 %016llx clamp %016llx tex0 %016llx regs", t.texture, t.local_texture,
                        t.gs_known ? 1 : 0, mem_prim, static_cast<unsigned long long>(tex1), static_cast<unsigned long long>(clamp), static_cast<unsigned long long>(tex0));
            for (uint32_t q = 0; q < 4 && mem_prim != 0; ++q) std::printf(" %08x%08x:%02x", lwu(mem_prim + 16 * q + 4), lwu(mem_prim + 16 * q), lbu(mem_prim + 16 * q + 8));
            std::printf(" (asset regs %02x %02x %02x %02x)\n", t.reg[0], t.reg[1], t.reg[2], t.reg[3]);
        }
        pi.word |= 1;
        return pi;
    }
    pi.bucket = g_level.tex_bucket[static_cast<size_t>(texture)];
    pi.layer = static_cast<uint32_t>(g_level.tex_layer[static_cast<size_t>(texture)]);
    const uint32_t nm = static_cast<uint32_t>(std::max<size_t>(1, g_level.textures[static_cast<size_t>(texture)].mips.size()));
    if (mxl + 1 > nm) ++g_st.mxl_over_mips;
    // the levels the GS may sample: at most TEX1's MXL + 1 (a texture without mips (MXL 0) has no
    // level 1 in the asset either: the bytes after its image are another texture's - Clank's head on Novalis)
    pi.word |= (std::min(std::min(nm, mxl + 1), 4u) - 1) << 22;
    return pi;
}

// orig: (host) the class converted to GPU vertices / triangle runs (appended to the level's buffers)
std::unique_ptr<ClassGpu> build_class(std::unique_ptr<mrp::MobyClass> asset, uint32_t class_ptr) {
    auto cg = std::make_unique<ClassGpu>();
    const mrp::MobyClass& c = *asset;
    cg->packets.resize(c.packets.size());
    std::vector<std::vector<SlotRec>> slots(c.packets.size());
    resolve_lod(c, 0, slots);
    resolve_lod(c, 1, slots);
    const uint32_t ptab = class_ptr != 0 ? lwu(class_ptr) : 0;
    for (size_t pi = 0; pi < c.packets.size(); ++pi) {
        const mrp::Packet& p = c.packets[pi];
        PacketGpu& pg = cg->packets[pi];
        if (p.lod == 2) {  // the shadow mesh: not drawn by this renderer (out of scope); its texture switches still
                           // change the GS's TEX0 the next packets drawn may start with (its last prim: no kick program traced)
            const uint32_t vif = ptab != 0 ? lwu(ptab + static_cast<uint32_t>(pi) * 0x10) : 0;
            for (size_t k = 0; k < p.textures.size(); ++k) {
                bool fm = false;
                pg.prims.push_back(prim_info(p.textures[k], vif != 0 && vif < 0x02000000u ? vif + p.vif_tex_off + static_cast<uint32_t>(k) * 0x40 : 0, fm));
            }
            pg.last_prim = static_cast<int>(p.textures.size()) - 1;
            continue;
        }
        ++g_st.packets;
        pg.tint_part = c.tint_first[p.lod] != 0xff && p.index_in_lod >= c.tint_first[p.lod];
        const uint32_t vif = ptab != 0 ? lwu(ptab + static_cast<uint32_t>(pi) * 0x10) : 0;
        for (size_t k = 0; k < p.textures.size(); ++k) {
            const uint32_t mem_prim = vif != 0 && vif < 0x02000000u ? vif + p.vif_tex_off + static_cast<uint32_t>(k) * 0x40 : 0;
            bool fm = false;
            PrimInfo info = prim_info(p.textures[k], mem_prim, fm);
            if (fm) cg->from_memory_prims = true;
            if (g_env_debug && info.game_texture >= static_cast<int>(g_level.level_textures))
                std::printf("[mrp] class %d packet %zu prim %zu: texture %d (past the level's %zu: the armor's, file %d) -> array %d layer %u\n", c.o_class, pi, k, info.game_texture,
                            g_level.level_textures, g_level.build_armor_file, info.bucket, info.layer);
            if (p.textures[k].gs_known && mem_prim != 0 && p.textures[k].texture >= 0)  // the asset's texture against the class in memory (CLAMP >> 24)
                for (uint32_t q = 0; q < 4; ++q)
                    if (lbu(mem_prim + 16 * q + 8) == 0x08 && static_cast<int>((lwu(mem_prim + 16 * q) >> 24) & 0xff) != p.textures[k].texture) ++g_st.prims_mismatch;
            pg.prims.push_back(info);
        }
        const std::vector<SlotRec>& sr = slots[pi];
        // the GPU vertices: one per (slot, q2 slot) the kick program sends
        std::map<uint16_t, uint32_t> vmap;
        pg.first_vertex = static_cast<uint32_t>(g_level.verts.size());
        auto vertex_of = [&](uint8_t slot, uint8_t q2) {
            const uint16_t key = static_cast<uint16_t>(slot | (q2 << 8));
            auto it = vmap.find(key);
            if (it != vmap.end()) return it->second;
            GpuVertex gv{};
            const SlotRec& r = sr[slot - 1u];
            std::memcpy(gv.pos, r.pos, sizeof gv.pos);
            gv.normal = r.normal;
            std::memcpy(gv.joint, r.joint, 3);
            gv.n = r.n;
            std::memcpy(gv.weight, r.weight, 3);
            std::memcpy(gv.rgba, r.rgba, 4);
            gv.uv[0] = p.uv[slot - 1u][0];
            gv.uv[1] = p.uv[slot - 1u][1];
            if (q2 != 0) {
                const SlotRec& r2 = sr[q2 - 1u];
                gv.flags = 1;
                std::memcpy(gv.q2pos, r2.pos, sizeof gv.q2pos);
                std::memcpy(gv.q2joint, r2.joint, 3);
                gv.q2n = r2.n;
                std::memcpy(gv.q2weight, r2.weight, 3);
                ++g_st.q2_vertices;
            }
            if (r.n == 0) ++g_st.unresolved_matrix;
            const uint32_t idx = static_cast<uint32_t>(g_level.verts.size());
            g_level.verts.push_back(gv);
            pg.vslots.push_back({slot, q2});
            vmap[key] = idx;
            return idx;
        };
        // the kick program walked as mrp::kick_triangles does: a Restart empties the strip, a
        // vertex with ADC closes no triangle, the texture in force is the last Texture op
        const size_t n = p.n_slots;
        uint32_t qv[2] = {0, 0};
        uint8_t qs[2] = {0, 0};
        int m = 0, tex = -1;
        std::vector<std::array<uint8_t, 3>> my_tris;
        std::vector<int> my_tex;
        for (const mrp::KickOp& op : p.kicks) {
            if (op.kind == mrp::KickKind::Restart) {
                m = 0;
                continue;
            }
            if (op.kind == mrp::KickKind::Texture) {
                tex = op.texture;
                continue;
            }
            if (op.pos == 0 || op.pos > n || op.col == 0 || op.col > n || op.uv == 0 || op.uv > n) continue;
            if (op.pos != op.col || op.pos != op.uv || (op.pos_q != op.pos && op.pos_q != 0xff) || (op.uv_q != op.pos && op.uv_q != 0xff)) ++g_st.mixed_ops;
            if (op.pos_q == 0xff || op.uv_q == 0xff) ++g_st.startq_ops;
            const uint8_t q2 = op.uv_q2 != 0 && op.uv_q2 <= n ? op.uv_q2 : 0;
            const uint32_t v = vertex_of(op.pos, q2);
            if (m >= 2 && !op.adc) {
                Run* run = pg.runs.empty() ? nullptr : &pg.runs.back();
                if (run == nullptr || run->prim != tex || run->first_index + run->count != g_level.indices.size()) {
                    pg.runs.push_back(Run{static_cast<uint32_t>(g_level.indices.size()), 0, tex});
                    run = &pg.runs.back();
                    if (tex < 0) ++g_st.prev_texture_runs;
                }
                g_level.indices.push_back(qv[0]);
                g_level.indices.push_back(qv[1]);
                g_level.indices.push_back(v);
                run->count += 3;
                ++g_st.triangles;
                my_tris.push_back({qs[0], qs[1], op.pos});
                my_tex.push_back(tex);
            }
            qv[0] = qv[1];
            qs[0] = qs[1];
            qv[1] = v;
            qs[1] = op.pos;
            m = std::min(m + 1, 2);
        }
        pg.last_prim = tex;
        pg.vertex_count = static_cast<uint32_t>(g_level.verts.size()) - pg.first_vertex;
        g_st.vertices += pg.vertex_count;
        if (g_env_check) {  // the triangles built here against the CPU reference's (slots and texture, in order)
            ++g_st.tri_check_packets;
            std::vector<std::array<int32_t, 3>> pos(n, {0, 0, 0});
            std::vector<std::array<int32_t, 4>> rgba(n, {0, 0, 0, 0});
            for (size_t s = 0; s < n; ++s) pos[s] = {static_cast<int32_t>(s), 0, static_cast<int32_t>(s) + 1};  // (any data: the slots are compared)
            mrp::Vu1Consts k{};
            k.proj = 1.0f;
            k.rows[0][0] = 1.0f;
            k.rows[1][1] = 1.0f;
            k.rows[2][2] = 1.0f;
            k.rows[2][3] = 1.0f;
            k.rows[3][3] = 1.0f;
            std::vector<mrp::GsTriangle> ref;
            mrp::kick_triangles(p, k, pos, rgba, ref, false);
            bool same = ref.size() == my_tris.size();
            for (size_t t = 0; same && t < ref.size(); ++t)
                same = ref[t].slot[0] == my_tris[t][0] && ref[t].slot[1] == my_tris[t][1] && ref[t].slot[2] == my_tris[t][2] && ref[t].texture == my_tex[t];
            if (!same) ++g_st.tri_check_bad;
        }
    }
    g_level.dirty = true;
    g_level.vk_dirty = true;
    ++g_st.classes;
    cg->asset = std::move(asset);
    return cg;
}

// ---- level loading -----------------------------------------------------------------------------

// orig: (host) the free hoverboard's class (Level::guest): the textures its prims name, read from its own level's
// textures.mrpt, appended to this level's and the prims renumbered to them
void guest_textures(Level& L) {
    std::vector<mrp::Texture> theirs;
    const int lvl = L.guest->level >= 0 ? L.guest->level : L.guest_key / 10000;
    if (!mrp::read_textures(asset_root() + "/" + mrpw::level_dir_name(lvl) + "/textures.mrpt", theirs)) {
        L.guest.reset();
        return;
    }
    std::map<int, int> remap;
    for (mrp::Packet& p : L.guest->packets)
        for (mrp::TexturePrim& t : p.textures) {
            if (t.texture < 0) continue;
            auto it = remap.find(t.texture);
            if (it == remap.end()) {
                if (static_cast<size_t>(t.texture) >= theirs.size()) {
                    t.gs_known = false;  // (none: drawn white)
                    continue;
                }
                mrp::Texture c = theirs[static_cast<size_t>(t.texture)];
                c.index = static_cast<uint32_t>(L.textures.size());
                it = remap.emplace(t.texture, static_cast<int>(L.textures.size())).first;
                L.textures.push_back(std::move(c));
            }
            t.texture = it->second;
        }
}

// orig: (host) the level's moby textures (textures.mrpt) decoded to RGBA (palette[CSM1 index], the GS's
// 0..0x80 alpha kept) into one texture array per size, every mip
void load_textures(Level& L) {
    const std::string path = asset_root() + "/" + mrpw::level_dir_name(L.id) + "/textures.mrpt";
    if (!mrp::read_textures(path, L.textures)) {
        std::fprintf(stderr, "[mrp] no moby textures %s\n", path.c_str());
        return;
    }
    // the armors' texture blocks (armor/<k>.mrpt, raw as the armor file holds them, 0x23fa00): a PIF2
    // header (+0x18 width, +0x1c height), the palette at +0x30 (0x400 bytes), the image at +0x430
    // (block 0: 0x14000 bytes, mips 0 and 1; block 1: 0x4000, mip 0), after the level's: 2 per file k
    L.level_textures = L.textures.size();
    L.armor_base = static_cast<int>(L.textures.size());
    for (int k = 0; k < 4; ++k) {
        std::vector<mrp::Texture> raw;
        mrp::read_textures(asset_root() + "/armor/" + std::to_string(k) + ".mrpt", raw);
        for (size_t s = 0; s < 2; ++s) {
            mrp::Texture t;
            t.index = static_cast<uint32_t>(L.textures.size());
            if (s < raw.size() && !raw[s].mips.empty() && raw[s].mips[0].size() >= 0x430) {
                const std::vector<uint8_t>& b = raw[s].mips[0];
                t.width = le32(&b[0x18]);  // (the armor file's bytes: little-endian)
                t.height = le32(&b[0x1c]);
                std::memcpy(t.palette.data(), &b[0x30], 0x400);  // (RGBA bytes, as Texture::palette holds them on any host)
                size_t at = 0x430;
                uint32_t w = t.width, h = t.height;
                while (w > 0 && h > 0 && w <= 1024 && h <= 1024 && at + static_cast<size_t>(w) * h <= b.size()) {
                    t.mips.emplace_back(b.begin() + static_cast<long>(at), b.begin() + static_cast<long>(at + static_cast<size_t>(w) * h));
                    at += static_cast<size_t>(w) * h;
                    w /= 2;
                    h /= 2;
                }
            }
            L.textures.push_back(std::move(t));
        }
    }
    if (L.guest != nullptr) guest_textures(L);
    L.tex_bucket.assign(L.textures.size(), -1);
    L.tex_layer.assign(L.textures.size(), 0);
    std::map<std::pair<uint32_t, uint32_t>, int> by_size;
    for (size_t i = 0; i < L.textures.size(); ++i) {
        const mrp::Texture& t = L.textures[i];
        if (t.width == 0 || t.height == 0 || t.mips.empty() || t.mips[0].size() < static_cast<size_t>(t.width) * t.height) continue;
        auto [it, fresh] = by_size.insert({{t.width, t.height}, static_cast<int>(L.buckets.size())});
        if (fresh) {
            Bucket b;
            b.w = t.width;
            b.h = t.height;
            L.buckets.push_back(b);
        }
        Bucket& b = L.buckets[static_cast<size_t>(it->second)];
        L.tex_bucket[i] = it->second;
        L.tex_layer[i] = static_cast<int>(b.textures.size());
        b.textures.push_back(static_cast<int>(i));
        b.levels = std::max<uint32_t>(b.levels, static_cast<uint32_t>(std::min<size_t>(t.mips.size(), 4)));
    }
    auto csm1 = [](uint32_t i) { return ((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i; };
    std::vector<uint32_t> px;
    for (Bucket& b : L.buckets) {
        uint32_t maxl = 1;
        while ((b.w >> maxl) > 0 && (b.h >> maxl) > 0 && maxl < b.levels) ++maxl;
        b.levels = std::max(1u, std::min(b.levels, maxl));
        const uint32_t layers = static_cast<uint32_t>(b.textures.size());
        if (vk::is_active()) {
            vk::create_image(b.w, b.h, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT, b.vk_tex, layers, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY, b.levels);
        } else {
            glGenTextures(1, &b.tex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &b.tex, "MRP mobys: texture arrays");
            glBindTexture(GL_TEXTURE_2D_ARRAY, b.tex);
            for (uint32_t l = 0; l < b.levels; ++l)
                glTexImage3D(GL_TEXTURE_2D_ARRAY, static_cast<GLint>(l), GL_RGBA8, static_cast<GLsizei>(std::max(1u, b.w >> l)), static_cast<GLsizei>(std::max(1u, b.h >> l)),
                             static_cast<GLsizei>(layers), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(b.levels) - 1);
        }
        for (size_t layer = 0; layer < b.textures.size(); ++layer) {
            const mrp::Texture& t = L.textures[static_cast<size_t>(b.textures[layer])];
            uint32_t pal[256];
            for (uint32_t i = 0; i < 256; ++i) pal[i] = t.palette[csm1(i)];
            for (uint32_t l = 0; l < b.levels; ++l) {
                const uint32_t w = std::max(1u, b.w >> l), h = std::max(1u, b.h >> l);
                px.assign(static_cast<size_t>(w) * h, 0u);
                if (l < t.mips.size() && t.mips[l].size() >= static_cast<size_t>(w) * h)
                    for (size_t k = 0; k < px.size(); ++k) px[k] = pal[t.mips[l][k]];
                if (vk::is_active()) {
                    vk::upload_image_2d(b.vk_tex, px.data(), px.size() * 4, 0, 0, static_cast<int>(w), static_cast<int>(h), 0, static_cast<uint32_t>(layer), l);
                } else {
                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, static_cast<GLint>(l), 0, 0, static_cast<GLint>(layer), static_cast<GLsizei>(w), static_cast<GLsizei>(h), 1, GL_RGBA,
                                    GL_UNSIGNED_BYTE, px.data());
                }
            }
        }
    }
    if (!vk::is_active()) glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void free_level(Level& L) {
    for (Bucket& b : L.buckets) {
        if (!vk::is_active() && b.tex != 0) glDeleteTextures(1, &b.tex);
        if (vk::is_active() && b.vk_tex.image != VK_NULL_HANDLE) vk::destroy_image(b.vk_tex);
    }
    if (!vk::is_active()) {
        if (L.vao != 0) glDeleteVertexArrays(1, &L.vao);
        if (L.vbo != 0) glDeleteBuffers(1, &L.vbo);
        if (L.ibo != 0) glDeleteBuffers(1, &L.ibo);
    } else {
        if (L.vk_vbo.buffer != VK_NULL_HANDLE) vk::destroy_buffer(L.vk_vbo);
        if (L.vk_ibo.buffer != VK_NULL_HANDLE) vk::destroy_buffer(L.vk_ibo);
    }
    L = Level{};
}

// orig: (host) the level's assets: every class file (by o_class), the armor, the textures
void load_level(int id) {
    free_level(g_level);
    Level& L = g_level;
    L.id = id;
    const std::string dir = asset_root() + "/" + mrpw::level_dir_name(id);
    if (!std::filesystem::is_directory(dir)) {
        std::fprintf(stderr, "[mrp] no MRP moby assets for level %d (%s): run setup.sh\n", id, dir.c_str());
        L.missing = true;
        return;
    }
    const double t0 = now_ms();
    for (const auto& de : std::filesystem::directory_iterator(dir)) {
        if (de.path().extension() != ".mrpm") continue;
        auto c = std::make_unique<mrp::MobyClass>();
        if (mrp::read_class(de.path().string(), *c)) L.files[c->o_class] = std::move(c);
    }
    const std::string ad = asset_root() + "/armor";
    if (std::filesystem::is_directory(ad))
        for (const auto& de : std::filesystem::directory_iterator(ad)) {
            if (de.path().extension() != ".mrpm") continue;
            auto c = std::make_unique<mrp::MobyClass>();
            if (mrp::read_class(de.path().string(), *c) && !c->packets.empty()) {
                L.armor.push_back(std::move(c));
                const std::string stem = de.path().stem().string();  // <k>_<s>
                L.armor_k.push_back(std::atoi(stem.c_str()));
                L.armor_s.push_back(stem.find('_') != std::string::npos ? std::atoi(stem.c_str() + stem.find('_') + 1) : 0);
            }
        }
    L.guest_key = g_mrp_guest_class.load();
    if (L.guest_key >= 0 && L.guest_key / 10000 != id) {  // (the free hoverboard's class, from its own level's assets)
        auto c = std::make_unique<mrp::MobyClass>();
        const std::string path = asset_root() + "/" + mrpw::level_dir_name(L.guest_key / 10000) + "/" + std::to_string(L.guest_key % 10000) + ".mrpm";
        if (mrp::read_class(path, *c) && !c->packets.empty()) L.guest = std::move(c);
        else std::fprintf(stderr, "[mrp] no guest class %s\n", path.c_str());
    }
    load_textures(L);
    if (vk::is_active()) {
        L.vk_loaded = true;
    } else {
        glGenVertexArrays(1, &L.vao);
        glGenBuffers(1, &L.vbo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &L.vbo, "MRP mobys: meshes");
        glGenBuffers(1, &L.ibo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &L.ibo, "MRP mobys: meshes");
        glBindVertexArray(L.vao);
        glBindBuffer(GL_ARRAY_BUFFER, L.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, L.ibo);
        const GLsizei S = sizeof(GpuVertex);
        glVertexAttribIPointer(0, 4, GL_SHORT, S, reinterpret_cast<void*>(offsetof(GpuVertex, pos)));
        glVertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(GpuVertex, joint)));
        glVertexAttribIPointer(2, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(GpuVertex, weight)));
        glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(GpuVertex, rgba)));
        glVertexAttribIPointer(4, 2, GL_SHORT, S, reinterpret_cast<void*>(offsetof(GpuVertex, uv)));
        glVertexAttribIPointer(5, 4, GL_SHORT, S, reinterpret_cast<void*>(offsetof(GpuVertex, q2pos)));
        glVertexAttribIPointer(6, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(GpuVertex, q2joint)));
        glVertexAttribIPointer(7, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(GpuVertex, q2weight)));
        for (GLuint a = 0; a < 8; ++a) glEnableVertexAttribArray(a);
        glBindVertexArray(0);
        L.gl_loaded = true;
    }
    L.loaded = true;
    std::printf("[mrp] level %d: %zu moby classes, %zu armor classes, %zu textures in %zu arrays (%.0f ms)\n", id, L.files.size(), L.armor.size(), L.textures.size(),
                L.buckets.size(), now_ms() - t0);
}

// orig: (host) the class header in EE memory -> its asset: the level's loaded classes (0x197300: headers
// by class index, 0x197a00: their o_class, 0x15ffc0 the count), the hero's armor by its first packet's
// vertex data (0x23fa00 puts the armor's classes in place of class 0's). `class_id` the moby's +0xa6: the weapons and gadgets are loaded
// on demand into one shared header (Veldin: 0x6d8000), which the class table lists under every weapon's o_class; the moby's own id picks
// the entry (by the header alone the table's last entry won, and a weapon was drawn with another's model)
ClassGpu* class_of(uint32_t class_ptr, int class_id) {
    Level& L = g_level;
    const uint32_t pt = lwu(class_ptr);
    const uint32_t vd = pt != 0 && pt < 0x02000000u ? lwu(pt + 8) : 0;  // the first packet's vertex data
    const uint64_t key = static_cast<uint64_t>(class_ptr) | (static_cast<uint64_t>(static_cast<uint16_t>(class_id)) << 32);
    auto it = L.by_ptr.find(key);
    if (it != L.by_ptr.end()) {
        if (it->second->vdata_ptr == vd) return it->second.get();
        L.by_ptr.erase(it);  // (the hero's classes replaced under the same header: another armor, 0x23fa00)
        L.miss.erase(key);
    }
    if (L.miss.count(key) != 0) return nullptr;
    const uint32_t n = lwu(OV(0x0015ffc0));
    int oc = -1;
    bool exact = false;  // (an entry with this header and the moby's own id)
    for (uint32_t cb = 0; cb < n && cb < 0x100; ++cb)
        if (lwu(OV(0x00197300) + cb * 4) == class_ptr && !exact) {
            oc = static_cast<int>(lhu(OV(0x00197a00) + cb * 2));
            exact = oc == class_id;
        }
    std::unique_ptr<mrp::MobyClass> asset;
    if (oc >= 0) {
        auto f = L.files.find(oc);
        if (f != L.files.end()) asset = std::make_unique<mrp::MobyClass>(*f->second);
    }
    int class_armor = -1;
    if (vd != 0 && vd < 0x02000000u)  // the armor's classes: their file k names the armor's two textures
        for (size_t i = 0; i < L.armor.size(); ++i) {
            const auto& v0 = L.armor[i]->packets[0].vdata;
            bool same = lbu(pt + 0xc) * 16u == v0.size();
            for (size_t b = 0; same && b < v0.size(); ++b) same = lbu(vd + static_cast<uint32_t>(b)) == v0[b];
            if (!same) continue;
            if (L.armor_s[i] == 0) L.armor_file = class_armor = L.armor_k[i];  // (the first class: its mesh is the file's own)
            if (g_env_debug) std::printf("[mrp] armor class matched by its vertex data: file %d class %d\n", L.armor_k[i], L.armor_s[i]);
            if (asset == nullptr) asset = std::make_unique<mrp::MobyClass>(*L.armor[i]);
            break;
        }
    if (asset == nullptr) {
        L.miss[key] = oc;
        if (g_env_debug) std::printf("[mrp] class header %08x (o_class %d): no asset\n", class_ptr, oc);
        return nullptr;
    }
    if (g_env_debug) std::printf("[mrp] class header %08x: o_class %d -> asset %d (%zu packets)\n", class_ptr, oc, asset->o_class, asset->packets.size());
    L.build_armor_file = class_armor >= 0 ? class_armor : L.armor_file;  // (the file the class's armor textures are)
    auto cg = build_class(std::move(asset), class_ptr);
    cg->vdata_ptr = vd;
    ClassGpu* r = cg.get();
    L.by_ptr[key] = std::move(cg);
    return r;
}

// ---- shaders -----------------------------------------------------------------------------------

const char* kVs = R"(#version 460 core
layout(location = 0) in ivec4 aPosN;
layout(location = 1) in uvec4 aJoint;
layout(location = 2) in uvec4 aWeight;
layout(location = 3) in uvec4 aRgba;
layout(location = 4) in ivec2 aUv;
layout(location = 5) in ivec4 aQ2Pos;
layout(location = 6) in uvec4 aQ2Joint;
layout(location = 7) in uvec4 aQ2Weight;

struct Part {
    vec4 clip[4];    // world mode: model -> GL clip (columns)
    vec4 rows[4];    // the VU1 rows R0, R1, R2, T (B+297..B+300)
    vec4 light[8];   // the skinning record's rows (+0x40..+0xbf)
    vec4 sub;        // the sub-mesh colour (bytes)
    ivec4 info;      // joint_first, joint_count, flags (1: sub-mesh colour), 0
};
layout(std430, binding = 0) readonly buffer Joints { vec4 J[]; };
layout(std430, binding = 1) readonly buffer Parts { Part parts[]; };
layout(std430, binding = 2) readonly buffer Cmds { uvec4 cmds[]; };  // x part, y texture word, z layer, w 0
layout(std430, binding = 3) readonly buffer Normals { vec2 normals[]; };

uniform int uMode;       // 0 the world's projection, 1 the VU1's onto the frame
uniform float uProj;     // B+292 x: Q = uProj / w
uniform vec4 uOffset;    // B+296
uniform vec4 uFrame;     // fb width, fb height, x scale, 1 a render target (its rows not flipped)
uniform vec4 uXyOff;     // XYOFFSET x, y (1/16 px), depth a, depth b
uniform float uFogMin;   // B+291 x: the fog's floor
)" RAC1_SHADOW_VS R"(
noperspective out vec4 vColor;
noperspective out vec3 vStq;
noperspective out float vFog;  // the XYZF2's F byte (the GS interpolates it along the triangle)
flat out uvec4 vCmd;
// the shader check (transform feedback)
flat out ivec4 tfPos;
flat out ivec4 tfCol;
flat out vec4 tfStq;
flat out ivec4 tfXyz;
// (the modern lighting: the colour without the key light, the record's first direction; declared last, after the original's
// varyings: their interpolation unchanged)
#ifdef SH_SPLIT
noperspective out vec4 vColorNoKey;
#endif

vec4 joint_row(uint j, int r, int first, int count) {
    return int(j) < count ? J[(first + int(j)) * 4 + r] : vec4(0.0);
}
// the VU0 matrix of a slot (mrp::skin_packet): a joint, or the blend M[j0] w0 + M[j1] w1 (+ M[j2] w2)
void matrix_of(uvec4 jt, uvec4 wt, int first, int count, out vec4 m[4]) {
    if (jt.w == 1u) {
        for (int r = 0; r < 4; ++r) m[r] = joint_row(jt.x, r, first, count);
        return;
    }
    if (jt.w == 0u) {
        for (int r = 0; r < 4; ++r) m[r] = vec4(0.0);
        return;
    }
    float w0 = float(wt.x) * (1.0 / 4096.0) * 16.0, w1 = float(wt.y) * (1.0 / 4096.0) * 16.0, w2 = float(wt.z) * (1.0 / 4096.0) * 16.0;
    for (int r = 0; r < 4; ++r) {
        precise vec4 a = joint_row(jt.x, r, first, count) * w0;
        a = a + joint_row(jt.y, r, first, count) * w1;
        if (jt.w == 3u) a = a + joint_row(jt.z, r, first, count) * w2;
        m[r] = a;
    }
}
// the position: ((R0 x + R1 y) + R2 z) + R3, truncated (FTOI0, saturating), the EE's s16
ivec3 skin_pos(vec4 m[4], ivec3 p) {
    ivec3 q;
    for (int l = 0; l < 3; ++l) {
        precise float a = m[0][l] * float(p.x);
        a = a + m[1][l] * float(p.y);
        a = a + m[2][l] * float(p.z);
        a = a + m[3][l];
        int i = int(clamp(a, -2147483648.0, 2147483520.0));
        q[l] = (i << 16) >> 16;
    }
    return q;
}
// FTOI4, saturating
int ftoi4(float f) {
    return int(clamp(f * 16.0, -2147483648.0, 2147483520.0));
}

void main() {
    uvec4 cmd = cmds[gl_BaseInstance];
    Part P = parts[cmd.x];
    int first = P.info.x, count = P.info.y;
    vec4 m[4];
    matrix_of(aJoint, aWeight, first, count, m);
    ivec3 q = skin_pos(m, aPosN.xyz);

    // the lighting (skin_packet: the VU0 program's second half and the EE's colour step)
    uint nb = uint(aPosN.w) & 0xffffu;
    vec2 ta = normals[nb & 0xffu], te = normals[(nb >> 8) & 0xffu];
    precise float nx = ta.x * te.x;
    precise float ny = ta.y * te.x;
    float nw = te.y;
    precise vec4 n = m[0] * nx;
    n = n + m[1] * ny;
    n = n + m[2] * nw;
    precise float len2 = n.x * n.x + n.y * n.y;
    len2 = len2 + n.z * n.z;
    float qn = len2 > 0.0 ? 1.0 / sqrt(len2) : 3.4028235e38;
    precise vec4 d = P.light[0] * n.x;
    d = d + P.light[1] * n.y;
    d = d + P.light[2] * n.z;
    precise vec4 dk = d * (-abs(P.light[6]));
    d = max(d, dk);
    precise vec4 a = P.light[3] * d.x;
    a = a + P.light[4] * d.y;
    a = a + P.light[5] * d.z;
    ivec4 col;
    for (int l = 0; l < 4; ++l) {
        precise float c = (a[l] == 0.0 ? 0.0 : a[l] * qn) + P.light[7][l] * 1.0;
        int h = int(floatBitsToUint(c) << 16) >> 16;
        int pr = clamp(h * int(aRgba[l]), -32768, 32767);
        col[l] = (pr >> 7) & 0xff;
    }
    if ((P.info.z & 1) != 0) col = ivec4(P.sub);
#ifdef SH_SPLIT
    vColorNoKey = vec4(col) / 255.0;
    if (uShOn != 0 && (P.info.z & 1) == 0) {  // the same without the key light's term: the shadowed colour
        // (the products recomputed from the record's rows, not from the original's `d`: the original's colour keeps its own operations)
        vec4 dn = P.light[0] * n.x + P.light[1] * n.y + P.light[2] * n.z;
        dn = max(dn, dn * (-abs(P.light[6])));
        vec4 an = P.light[4] * dn.y + P.light[5] * dn.z;
        ivec4 cn;
        for (int l = 0; l < 4; ++l) {
            precise float c = (an[l] == 0.0 ? 0.0 : an[l] * qn) + P.light[7][l] * 1.0;
            int h = int(floatBitsToUint(c) << 16) >> 16;
            int pr = clamp(h * int(aRgba[l]), -32768, 32767);
            cn[l] = (pr >> 7) & 0xff;
        }
        vColorNoKey = vec4(cn) / 255.0;
    }
#endif

    // the VU1 program (micro 0x22d, mrp::vu1_vertex / kick_triangles)
    precise vec4 p = P.rows[3];
    p = p + P.rows[0] * float(q.x);
    p = p + P.rows[1] * float(q.y);
    p = p + P.rows[2] * float(q.z);
    float Q = uProj / p.w;
    float qq = Q;
    if ((aWeight.w & 1u) != 0u) {  // STQ x Q twice: the Q of another slot
        vec4 m2[4];
        matrix_of(aQ2Joint, aQ2Weight, first, count, m2);
        ivec3 q2 = skin_pos(m2, aQ2Pos.xyz);
        precise float w2 = P.rows[3].w;
        w2 = w2 + P.rows[0].w * float(q2.x);
        w2 = w2 + P.rows[1].w * float(q2.y);
        w2 = w2 + P.rows[2].w * float(q2.z);
        qq = Q * (uProj / w2);
    }
    vec3 stq = vec3(float(aUv.x) * (1.0 / 4096.0) * qq, float(aUv.y) * (1.0 / 4096.0) * qq, 1.0 * qq);
    precise float X = p.x * Q + uOffset.x;
    precise float Y = p.y * Q + uOffset.y;
    precise float Z = p.z * Q + uOffset.z;
    int xi = ftoi4(X), yi = ftoi4(Y), zi = ftoi4(Z);

    if (uMode == 0) {
        gl_Position = sh_clip(mat4(P.clip[0], P.clip[1], P.clip[2], P.clip[3]) * vec4(vec3(q), 1.0));
    } else {
        vShWorld = vec3(0.0);  // the GS's frame: x = (X16 - XYOFFSET) / 16 px (the VU's 1/16 px snap), the host's depth a + b Z
        // as homogeneous coordinates with w = 1 / Q = p.w / proj (x w, y w, z w all linear in p) so that the GPU
        // clips at the near plane what the VU's clipping program (0x00a) clips; behind the camera (w <= 0) no snap
        float wq = p.w / uProj;
        float X1 = uFrame.x * 0.5;
        float sx = uFrame.z / X1, sy = 1.0 / (uFrame.y * 0.5);
        vec2 c = vec2(uXyOff.x / 16.0 + X1, uXyOff.y / 16.0 + uFrame.y * 0.5);  // the frame's centre in GS units
        vec2 xy = wq > 0.0 ? vec2(float(xi), float(yi)) / 16.0 * wq : vec2(p.x + uOffset.x * wq, p.y + uOffset.y * wq);
        float zc = (2.0 * (uXyOff.z + uXyOff.w * uOffset.z) - 1.0) * wq + 2.0 * uXyOff.w * p.z;  // (2 (a + b Z) - 1) w, Z = p.z Q + offset.z
        gl_Position = vec4((xy.x - c.x * wq) * sx, (uFrame.w != 0.0 ? 1.0 : -1.0) * (xy.y - c.y * wq) * sy, zc, wq);  // (uFrame.w: a render target's rows)
    }
    // the fog: the VU1 program's F = min(max(T.w + offset.w, floor), 255) (the moby's, micro 0x22d) or, in the clipping program (0x00a: info.w bit 0),
    // min(max(p.w + offset.w, floor), 255) per vertex; the XYZF2's F is the integer part
    vFog = floor(min(max(((P.info.w & 1) != 0 ? p.w : P.rows[3].w) + uOffset.w, uFogMin), 255.0));
    vColor = vec4(col) / 255.0;
    vStq = stq;
    vCmd = cmd;
    tfPos = ivec4(q, 0);
    tfCol = col;
    tfStq = vec4(stq, p.w);
    tfXyz = ivec4(xi, yi, zi, int(vFog));
}
)";

// the GS's texture functions and alpha test (as runtime/gs/gs_draw.cpp's), the texture level by the
// GS's rule from Q (TEX1 LCM / L / K, MXL) or the GPU's trilinear choice over every mip
const char* kFs = R"(#version 460 core
noperspective in vec4 vColor;
noperspective in vec3 vStq;
noperspective in float vFog;
flat in uvec4 vCmd;
#ifdef SH_SPLIT
noperspective in vec4 vColorNoKey;  // (last)
#endif
uniform sampler2DArray uTex;
uniform int uTme, uAte, uAtst, uAref, uAfail, uMipMode, uShade, uFge;
// a batch with Z writes whose alpha test's fail mode writes one of colour / Z only (AFAIL FB_ONLY / RGB_ONLY / ZB_ONLY) is
// drawn twice: 0 the colour (no Z writes), 1 the Z (colour masked), each pass keeping the pixels the GS writes that buffer for
uniform int uPass;
uniform vec3 uFogCol;    // FOGCOL (the GS register), 0..1
)" RAC1_SHADOW_FS R"(
out vec4 FragColor;
void main() {
    sh_prepare();
    vec4 c = vColor;
#ifdef SH_SPLIT
    vec3 c_game = c.rgb;  // (the game's colour: an emissive texel's, sh_emissive)
    c.rgb = sh_apply_split(c.rgb, vColorNoKey.rgb);  // the key light's term where the sun is hidden
    // (the mobys keep the game's point lights - the record's third direction, per vertex: small meshes, and the effects'
    // shells - explosions - are coloured by them; the world takes them per pixel, sh_points). Ours (placed by hand, per
    // class, the emitters) per pixel as the world's: sh_points_moby leaves the game's out (#110)
    c.rgb = sh_points_moby(c.rgb);
#else
    c.rgb = sh_apply(c.rgb);
#endif
    vec4 o = c;
    uint w = vCmd.y;
    if (uTme != 0) {
        vec2 st = vStq.xy / vStq.z;
#ifdef SH_SPLIT
        st = sh_mat_uv(st, (w >> 27) & 31u);  // (lava, mud, quicksand: the slow flow)
#endif
        vec4 t;
        if ((w & 1u) != 0u) {
            t = vec4(128.0 / 255.0);
        } else {
            int mips = int((w >> 22) & 7u) + 1;
            float layer = float(vCmd.z);
            if (uMipMode == 0) {
                int mxl = int((w >> 4) & 7u), lcm = int((w >> 7) & 1u), L = int((w >> 8) & 3u);
                float K = float((int(w >> 10) << 20) >> 20) / 16.0;
                float lod = lcm != 0 ? K : log2(1.0 / max(abs(vStq.z), 1e-20)) * float(1 << L) + K;
                int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), mxl);
                level = min(level, mips - 1);
                t = textureLod(uTex, vec3(st, layer), float(level));
                if (level == 0 && (w & (1u << 25)) != 0u) { FragColor = vec4(1.0, 0.0, 1.0, 1.0); return; }
                if (level == 1 && (w & (1u << 26)) != 0u) { FragColor = vec4(0.0, 1.0, 1.0, 1.0); return; }
            } else {
                float lod = clamp(textureQueryLod(uTex, st).y, 0.0, float(mips - 1));
                t = textureLod(uTex, vec3(st, layer), lod);
            }
        }
        uint tfx = (w >> 1) & 3u, tcc = (w >> 3) & 1u;
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
#ifdef SH_SPLIT
        o.rgb = sh_emissive(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, (w >> 27) & 31u);  // (the word's bits 27..31)
        o.rgb = sh_material(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, tfx == 1u ? vec3(1.0) : c.rgb * (255.0 / 128.0), (w >> 27) & 31u);
#endif
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    else if (uShade == 3) o.rgb = vec3(0.8);
    o = clamp(o, 0.0, 1.0);
    if (uFge != 0) {  // the GS's fogging (PRIM FGE) after the texture function: Cv F + FOGCOL (1 - F), F the byte / 255
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + uFogCol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (uAte != 0) {
        bool pass = true;
        if (uAtst == 0) pass = false;
        else if (uAtst == 2) pass = a < float(uAref);
        else if (uAtst == 3) pass = a <= float(uAref);
        else if (uAtst == 4) pass = abs(a - float(uAref)) < 0.5;
        else if (uAtst == 5) pass = a >= float(uAref);
        else if (uAtst == 6) pass = a > float(uAref);
        else if (uAtst == 7) pass = abs(a - float(uAref)) >= 0.5;
        if (!pass && uShPass != 0) discard;  // (the shadow map: the cut-outs' passing pixels cast)
        if (!pass && uPass == 0 && (uAfail == 0 || uAfail == 2)) discard;
        if (!pass && uPass == 1 && uAfail != 2) discard;
    }
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
)";

GLuint g_prog = 0, g_tf_prog = 0;
// the modern lighting's variant (SH_SPLIT: the colour without the key light too, a varying more): only while the lighting is
// on, so that the original's program - its interpolation - stays as it was
GLuint g_prog_split = 0;
GLuint g_ssbo[4] = {}, g_indirect = 0, g_tf_buf = 0;
GLuint g_samplers[2][16] = {};  // [mip mode][sampler bits]
struct Loc {
    GLint mode, proj, offset, frame, xyoff, tex, tme, ate, atst, aref, afail, mip, shade, fogmin, fge, fogcol, pass;
    ShadowLocs sh;  // (the modern lighting, mrp_lighting.h)
} g_loc{}, g_tf_loc{}, g_loc_split{};
// orig: (host) the draw's program and locations swapped for the lighting's variant while it is on (restored at the scope's end)
struct SplitScope {
    bool on = false;
    SplitScope();
    ~SplitScope();
};

GLuint compile(GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrp] shader: %s\n", log);
    }
    return s;
}
Loc locs(GLuint p) {
    Loc l;
    l.mode = glGetUniformLocation(p, "uMode");
    l.proj = glGetUniformLocation(p, "uProj");
    l.offset = glGetUniformLocation(p, "uOffset");
    l.frame = glGetUniformLocation(p, "uFrame");
    l.xyoff = glGetUniformLocation(p, "uXyOff");
    l.tex = glGetUniformLocation(p, "uTex");
    l.tme = glGetUniformLocation(p, "uTme");
    l.ate = glGetUniformLocation(p, "uAte");
    l.atst = glGetUniformLocation(p, "uAtst");
    l.aref = glGetUniformLocation(p, "uAref");
    l.afail = glGetUniformLocation(p, "uAfail");
    l.pass = glGetUniformLocation(p, "uPass");
    l.mip = glGetUniformLocation(p, "uMipMode");
    l.shade = glGetUniformLocation(p, "uShade");
    l.fogmin = glGetUniformLocation(p, "uFogMin");
    l.fge = glGetUniformLocation(p, "uFge");
    l.fogcol = glGetUniformLocation(p, "uFogCol");
    l.sh = mrp_lighting_locs(p);
    return l;
}

bool init_gl() {
    if (g_prog != 0) return true;
    p_multi_draw_elements_indirect = reinterpret_cast<PfnMultiDrawElementsIndirect>(glfwGetProcAddress("glMultiDrawElementsIndirect"));
    p_draw_arrays_instanced_base_instance = reinterpret_cast<PfnDrawArraysInstancedBaseInstance>(glfwGetProcAddress("glDrawArraysInstancedBaseInstance"));
    if (p_multi_draw_elements_indirect == nullptr || p_draw_arrays_instanced_base_instance == nullptr) {
        std::fprintf(stderr, "[mrp] no OpenGL 4.6 entry points\n");
        return false;
    }
    std::printf("[mrp] GL %s, %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)), reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    const GLuint vs = compile(GL_VERTEX_SHADER, kVs), fs = compile(GL_FRAGMENT_SHADER, kFs);
    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    GLint ok = 0;
    glGetProgramiv(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(g_prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrp] link: %s\n", log);
        return false;
    }
    g_loc = locs(g_prog);
    {  // the lighting's variant
        auto split = [](const char* src) {
            std::string t = src;
            const std::string v = "#version 460 core\n";
            if (t.compare(0, v.size(), v) == 0) t.insert(v.size(), "#define SH_SPLIT 1\n");
            return t;
        };
        const std::string vss = split(kVs), fss = split(kFs);
        const GLuint v3 = compile(GL_VERTEX_SHADER, vss.c_str()), f3 = compile(GL_FRAGMENT_SHADER, fss.c_str());
        g_prog_split = glCreateProgram();
        glAttachShader(g_prog_split, v3);
        glAttachShader(g_prog_split, f3);
        glLinkProgram(g_prog_split);
        GLint ok3 = 0;
        glGetProgramiv(g_prog_split, GL_LINK_STATUS, &ok3);
        if (!ok3) {
            char log[2048];
            glGetProgramInfoLog(g_prog_split, sizeof log, nullptr, log);
            std::fprintf(stderr, "[mrp] link (lighting variant): %s\n", log);
            g_prog_split = 0;
        } else {
            g_loc_split = locs(g_prog_split);
        }
    }
    // the shader check's program: the same vertex shader, its outputs captured
    const GLuint vs2 = compile(GL_VERTEX_SHADER, kVs);
    g_tf_prog = glCreateProgram();
    glAttachShader(g_tf_prog, vs2);
    const char* outs[] = {"tfPos", "tfCol", "tfStq", "tfXyz"};
    glTransformFeedbackVaryings(g_tf_prog, 4, outs, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(g_tf_prog);
    g_tf_loc = locs(g_tf_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteShader(vs2);
    glGenBuffers(4, g_ssbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 4, g_ssbo, "MRP mobys: frame buffers");
    glGenBuffers(1, &g_indirect);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_indirect, "MRP mobys: frame buffers");
    glGenBuffers(1, &g_tf_buf);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_tf_buf, "MRP mobys: frame buffers");
    float aniso = 1.0f;
    glGetFloatv(kGlMaxAnisotropy, &aniso);
    for (int mm = 0; mm < 2; ++mm)
        for (uint32_t s = 0; s < 16; ++s) {
            GLuint& sm = g_samplers[mm][s];
            glGenSamplers(1, &sm);
            const bool mag = (s & 1) != 0, mn = (s & 2) != 0;
            glSamplerParameteri(sm, GL_TEXTURE_MAG_FILTER, mag ? GL_LINEAR : GL_NEAREST);
            if (mm == 0) glSamplerParameteri(sm, GL_TEXTURE_MIN_FILTER, mn ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST);
            else {
                glSamplerParameteri(sm, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glSamplerParameterf(sm, kGlMaxAnisotropy, std::min(aniso, 16.0f));
            }
            glSamplerParameteri(sm, GL_TEXTURE_WRAP_S, (s & 4) != 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sm, GL_TEXTURE_WRAP_T, (s & 8) != 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        }
    return true;
}

struct MobyVkPush {
    float uOffset[4];
    float uFrame[4];
    float uXyOff[4];
    float uFogCol_Min[4];
    int32_t uMode;
    float uProj;
    int32_t uTme;
    int32_t uAte;
    int32_t uAtst;
    int32_t uAref;
    int32_t uAfail;
    int32_t uMipMode;
    int32_t uShade;
    int32_t uFge;
    int32_t uPass;
    int32_t _pad = 0;
};
static_assert(sizeof(MobyVkPush) == 112);

struct MobyVkPipeKey {
    bool split_prog = false;
    bool caster = false;
    bool depth_clamp = false;
    bool depth_test = false;
    bool depth_write = false;
    VkCompareOp depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkColorComponentFlags color_mask = 0xF;
    uint64_t prim = 0;
    uint64_t alpha = 0;
    VkFormat color_fmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_fmt = VK_FORMAT_D32_SFLOAT_S8_UINT;
    bool has_stencil = true;
    bool ao_mark = false;  // (the pixels drawn set the stencil to 1: the ambient occlusion's mobys, mrp_lighting_ao_moby_mark)
    bool operator==(const MobyVkPipeKey&) const = default;
};

struct MobyVkState {
    bool inited = false;
    bool shutdown_reg = false;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipe_layout = VK_NULL_HANDLE;
    VkShaderModule vs = VK_NULL_HANDLE;
    VkShaderModule fs = VK_NULL_HANDLE;
    VkShaderModule split_vs = VK_NULL_HANDLE;
    VkShaderModule split_fs = VK_NULL_HANDLE;
    std::vector<std::pair<MobyVkPipeKey, VkPipeline>> pipes;
    vk::RingAlloc joints_alloc{};
    vk::RingAlloc parts_alloc{};
    vk::RingAlloc cmds_alloc{};
    vk::RingAlloc normals_alloc{};
    vk::RingAlloc indirect_alloc{};
} g_vk_moby;

void moby_vk_shutdown() {
    VkDevice dev = vk::device();
    for (Bucket& b : g_level.buckets) {
        if (b.vk_tex.image != VK_NULL_HANDLE) vk::destroy_image_now(b.vk_tex);
    }
    if (g_level.vk_vbo.buffer != VK_NULL_HANDLE) vk::destroy_buffer_now(g_level.vk_vbo);
    if (g_level.vk_ibo.buffer != VK_NULL_HANDLE) vk::destroy_buffer_now(g_level.vk_ibo);
    g_level.vk_loaded = false;
    if (g_level.id >= 0 && !g_level.gl_loaded) g_level.id = -1;

    for (auto& [k, p] : g_vk_moby.pipes) {
        if (p != VK_NULL_HANDLE) vkDestroyPipeline(dev, p, nullptr);
    }
    g_vk_moby.pipes.clear();
    if (g_vk_moby.vs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_moby.vs, nullptr);
    if (g_vk_moby.fs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_moby.fs, nullptr);
    if (g_vk_moby.split_vs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_moby.split_vs, nullptr);
    if (g_vk_moby.split_fs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_moby.split_fs, nullptr);
    if (g_vk_moby.pipe_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(dev, g_vk_moby.pipe_layout, nullptr);
    if (g_vk_moby.set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(dev, g_vk_moby.set_layout, nullptr);
    const bool reg = g_vk_moby.shutdown_reg;
    g_vk_moby = MobyVkState{};
    g_vk_moby.shutdown_reg = reg;
}

bool init_vk() {
    if (g_vk_moby.inited) return true;
    VkDevice dev = vk::device();
    if (dev == VK_NULL_HANDLE) return false;
    if (!g_vk_moby.shutdown_reg) {
        vk::register_shutdown_callback(moby_vk_shutdown);
        g_vk_moby.shutdown_reg = true;
    }

    VkDescriptorSetLayoutBinding bindings[5]{};
    for (uint32_t i = 0; i < 4; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 5;
    dslci.pBindings = bindings;
    vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &g_vk_moby.set_layout);

    VkDescriptorSetLayout set_layouts[2] = {
        g_vk_moby.set_layout,
        reinterpret_cast<VkDescriptorSetLayout>(mrp_lighting_vk_set_layout()),
    };
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(MobyVkPush);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 2;
    plci.pSetLayouts = set_layouts;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    vkCreatePipelineLayout(dev, &plci, nullptr, &g_vk_moby.pipe_layout);

    g_vk_moby.vs = vk::create_shader_module(vk::shaders::kMrpMobyVs, sizeof(vk::shaders::kMrpMobyVs));
    g_vk_moby.fs = vk::create_shader_module(vk::shaders::kMrpMobyFs, sizeof(vk::shaders::kMrpMobyFs));
    g_vk_moby.split_vs = vk::create_shader_module(vk::shaders::kMrpMobySplitVs, sizeof(vk::shaders::kMrpMobySplitVs));
    g_vk_moby.split_fs = vk::create_shader_module(vk::shaders::kMrpMobySplitFs, sizeof(vk::shaders::kMrpMobySplitFs));
    g_vk_moby.inited = true;
    return true;
}

VkPipeline get_moby_vk_pipeline(const MobyVkPipeKey& key) {
    for (const auto& [k, p] : g_vk_moby.pipes) {
        if (k == key) return p;
    }
    vk::GraphicsPipelineDesc desc{};
    desc.layout = g_vk_moby.pipe_layout;
    desc.vs = key.split_prog ? g_vk_moby.split_vs : g_vk_moby.vs;
    desc.fs = key.split_prog ? g_vk_moby.split_fs : g_vk_moby.fs;
    desc.vertex_bindings = {
        {0, sizeof(GpuVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    desc.vertex_attribs = {
        {0, 0, VK_FORMAT_R16G16B16A16_SINT, static_cast<uint32_t>(offsetof(GpuVertex, pos))},
        {1, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(GpuVertex, joint))},
        {2, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(GpuVertex, weight))},
        {3, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(GpuVertex, rgba))},
        {4, 0, VK_FORMAT_R16G16_SINT, static_cast<uint32_t>(offsetof(GpuVertex, uv))},
        {5, 0, VK_FORMAT_R16G16B16A16_SINT, static_cast<uint32_t>(offsetof(GpuVertex, q2pos))},
        {6, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(GpuVertex, q2joint))},
        {7, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(GpuVertex, q2weight))},
    };
    desc.cull_mode = VK_CULL_MODE_NONE;
    desc.depth_clamp = key.depth_clamp;
    desc.depth_test = key.depth_test;
    desc.depth_write = key.depth_write;
    desc.depth_compare = key.depth_compare;
    desc.color_write_mask = key.color_mask;
    desc.color_format = key.color_fmt;
    desc.depth_format = key.depth_fmt;
    desc.has_depth_attachment = key.depth_fmt != VK_FORMAT_UNDEFINED;
    desc.has_stencil_attachment = key.has_stencil;
    if (key.ao_mark && key.has_stencil) {
        desc.stencil_test = true;
        desc.stencil_front = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xff, 0xff, 1};
        desc.stencil_back = desc.stencil_front;
    }
    if (key.caster) {
        desc.depth_bias_enable = true;
        desc.extra_dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    } else {
        gs::DrawState ds{};
        ds.prim = key.prim;
        ds.alpha = key.alpha;
        float fix = 0.0f;
        gs::vk_blend_state(ds, false, desc, fix);
    }
    VkPipeline pipe = vk::create_graphics_pipeline(desc);
    g_vk_moby.pipes.emplace_back(key, pipe);
    return pipe;
}

void bind_moby_vk_set0(VkCommandBuffer cb, int bucket, uint32_t sampler_bits, int mm) {
    // (the frame's sets kept by what they bind - the texture array, the sampler, the frame's buffers -: a batch binding the same
    // as one before, the casters' passes' too, reuses its set; the transient pools are reset at each frame's start, the cache with them)
    static uint64_t cache_frame = ~0ull;
    static std::map<std::array<uint64_t, 10>, VkDescriptorSet> cache;
    if (cache_frame != vk::frame_number()) {
        cache_frame = vk::frame_number();
        cache.clear();
    }
    const vk::RingAlloc* rb[4] = {&g_vk_moby.joints_alloc, &g_vk_moby.parts_alloc, &g_vk_moby.cmds_alloc, &g_vk_moby.normals_alloc};
    std::array<uint64_t, 10> key = {static_cast<uint64_t>(static_cast<int64_t>(bucket)), (sampler_bits & 15u) | (static_cast<uint64_t>(mm) << 8)};
    for (int i = 0; i < 4; ++i) {
        key[static_cast<size_t>(2 + i * 2)] = reinterpret_cast<uint64_t>(rb[i]->buffer);
        key[static_cast<size_t>(3 + i * 2)] = rb[i]->offset;
    }
    VkDescriptorSet& cached = cache[key];
    if (cached != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_moby.pipe_layout, 0, 1, &cached, 0, nullptr);
        return;
    }
    VkDescriptorSet set = vk::alloc_descriptor_set(g_vk_moby.set_layout);
    cached = set;
    VkDescriptorBufferInfo binfo[4] = {
        {g_vk_moby.joints_alloc.buffer, g_vk_moby.joints_alloc.offset, VK_WHOLE_SIZE},
        {g_vk_moby.parts_alloc.buffer, g_vk_moby.parts_alloc.offset, VK_WHOLE_SIZE},
        {g_vk_moby.cmds_alloc.buffer, g_vk_moby.cmds_alloc.offset, VK_WHOLE_SIZE},
        {g_vk_moby.normals_alloc.buffer, g_vk_moby.normals_alloc.offset, VK_WHOLE_SIZE},
    };
    const uint32_t s = sampler_bits & 15u;
    const bool mag = (s & 1u) != 0, mn = (s & 2u) != 0;
    const VkFilter mag_f = mag ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    const VkFilter min_f = (mm == 0) ? (mn ? VK_FILTER_LINEAR : VK_FILTER_NEAREST) : VK_FILTER_LINEAR;
    const VkSamplerMipmapMode mip_m = (mm == 0) ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    const float aniso = (mm == 0) ? 1.0f : 16.0f;
    const VkSamplerAddressMode ws = (s & 4u) != 0 ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    const VkSamplerAddressMode wt = (s & 8u) != 0 ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkDescriptorImageInfo iinfo{};
    iinfo.sampler = vk::get_sampler(min_f, mag_f, ws, wt, false, VK_COMPARE_OP_ALWAYS, aniso, mip_m, 1000.0f);
    if (bucket >= 0 && static_cast<size_t>(bucket) < g_level.buckets.size() && g_level.buckets[static_cast<size_t>(bucket)].vk_tex.view != VK_NULL_HANDLE) {
        iinfo.imageView = g_level.buckets[static_cast<size_t>(bucket)].vk_tex.view;
    } else {
        iinfo.imageView = vk::white_texture_array().view;
    }
    iinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[5]{};
    for (uint32_t i = 0; i < 4; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &binfo[i];
    }
    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = set;
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].pImageInfo = &iinfo;
    vkUpdateDescriptorSets(vk::device(), 5, writes, 0, nullptr);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_moby.pipe_layout, 0, 1, &set, 0, nullptr);
}

// ---- per-frame state ---------------------------------------------------------------------------

struct PartGpu {
    float clip[16];
    float rows[16];
    float light[32];
    float sub[4];
    int32_t info[4];
};
static_assert(sizeof(PartGpu) == 4 * (16 + 16 + 32 + 4 + 4));

struct DrawCmd {  // DrawElementsIndirectCommand
    uint32_t count, instances, first_index;
    int32_t base_vertex;
    uint32_t base_instance;
};

// the GL state of a run of draws
struct Key {
    int bucket = -1;
    uint32_t sampler = 0;
    uint64_t prim = 0, test = 0, alpha = 0, zbuf = 0;
    bool deferred = false;  // the draw loop's second pass (the mobys whose drop shadow is seen): drawn after the shadows
    bool caster_only = false;  // a moby outside the view (the casters' cache): drawn into the shadow maps only
    uint32_t rt = 0;  // a moby drawn into a render target (MrpMoby::rt): its address; drawn there by mrp_moby_draw_target only
    // the boot's title main menu's boxes (MrpMoby::other_view, Frame::title_menu): drawn in the game's own 4:3 projection (mode 1)
    // over the scene's mobys in the world's, cut at the 4:3 frame; never casters
    bool own_view = false;
    bool operator==(const Key&) const = default;
};
struct Batch {
    Key key;
    uint32_t first = 0, count = 0;  // into the frame's draw commands
};

struct Frame {
    bool active = false;
    bool title = false, menu = false, title_menu = false;
    std::vector<PartGpu> parts;
    std::vector<uint32_t> cmds;  // 4 words each
    std::vector<DrawCmd> draws;
    std::vector<Batch> batches;
    // per part: the moby, the class, the packet (the shader check)
    struct PartRef {
        uint32_t moby = 0;
        ClassGpu* cls = nullptr;
        uint32_t packet = 0, part = 0;
        bool tint = false;
        uint64_t test = 0, alpha = 0;  // the GS state the MRP draws the part with
    };
    std::vector<PartRef> refs;
    float proj = 0, offset[4] = {}, fog_min = 0;
    uint64_t prim = 0;
    uint64_t env_test = 0x5360b, env_alpha = 0x44, env_zbuf = 0, env_xyoff = 0;
    bool built = false;
    bool uploaded = false;
    int mode = 0;
    float normals[0x200] = {};
    std::vector<MrpMat4> joints_all;
    std::vector<MrpMat4> joints_extra;  // the cached casters' joints, after the frame's (mrp_scene's)
    std::vector<glm::vec3> part_pos;    // each part's world origin (a point light's cube takes the parts near it)
    std::vector<float> part_rad;        // a radius around it holding the part (its class's bounding sphere, generous)
    std::vector<uint8_t> part_own;      // 1: a part of an own_view moby (Key::own_view: no world place, not a caster)
    // the view of each moby drawn into a render target (MrpMoby::rt_proj): B+292 x, B+296, B+291 x as for the frame's
    struct RtView {
        float proj = 0, offset[4] = {}, fog_min = 0;
    };
    std::unordered_map<uint32_t, RtView> rt_views;
    // per moby drawn in the world's view (the light editor's: its pose for "emit moby ... joint <n>", the moby under the mouse):
    // its model -> world matrix (its first part's), its pose's joints in joints_all, its class's bounding sphere in the world
    struct MobyXf {
        uint32_t moby = 0;
        int16_t class_id = 0;
        glm::mat4 W{1.0f};
        uint32_t joint_first = 0, joint_count = 0;
        glm::vec3 centre{0.0f};
        float radius = 0.0f;
    };
    std::vector<MobyXf> xf;
};
Frame g_frame;
// set while mrp_moby_draw_target draws: the target's uniforms in place of the frame's
struct RtDraw {
    const Frame::RtView* view = nullptr;
    float w = 0, h = 0, xo = 0, yo = 0, da = 0, db = 0;
};
const RtDraw* g_rt_draw = nullptr;

// orig: (host) a radius around a part's origin holding it: its class's bounding sphere (class +0x30: centre, radius) by the
// part's scale, half again and a unit more (the animated poses, a cloth's swing); 4 units without a class
float part_radius(const ClassGpu* cg, const glm::dmat4& W) {
    if (cg == nullptr || cg->asset == nullptr) return 4.0f;
    const auto& b = cg->asset->bsphere;
    const float s = static_cast<float>(std::max({glm::length(glm::dvec3(W[0])), glm::length(glm::dvec3(W[1])), glm::length(glm::dvec3(W[2]))}));
    const float r = (std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]) + std::fabs(b[3])) * s;
    return std::isfinite(r) ? r * 1.5f + 1.0f : 4.0f;
}

// the modern lighting's mobys outside the view: the game skins and poses only the mobys it draws, so each drawn moby is
// kept (its parts' world matrices, joints, draws) and, while it stays alive, of its class and where it was, cast from that copy when
// the camera no longer sees it (a static moby - Ratchet's house, a crate - exactly; an animated one with its last pose)
struct CachedRun {
    uint32_t count = 0, first_index = 0, word = 0, layer = 0;
    Key key;
};
struct CachedPart {
    glm::dmat4 W{1.0};
    float rad = 0.0f;  // (Frame::part_rad)
    float rows[16] = {}, light[32] = {}, sub[4] = {};
    int32_t info2 = 0, info3 = 0;
    std::vector<CachedRun> runs;
};
struct CachedMoby {
    int16_t class_id = 0;
    float pos[3] = {};
    uint64_t seen = 0;
    std::vector<MrpMat4> joints;
    std::vector<CachedPart> parts;
};
std::unordered_map<uint32_t, CachedMoby> g_mcache;
uint64_t g_mcache_frame = 0;
uint32_t g_mcache_level = 0xffffffffu;
std::vector<size_t> g_prog_marks;  // the GS's primitive count before / after each faithful moby program start (the diff)

// orig: (host) the GS registers an A+D GIF packet at `a` writes (FLG PACKED, every REGS 0xe)
void gif_ad(uint32_t a, uint32_t qwc, std::map<uint8_t, uint64_t>& regs) {
    uint32_t off = 0;
    while (off < qwc) {
        const uint64_t lo = static_cast<uint64_t>(lwu(a + off * 16)) | (static_cast<uint64_t>(lwu(a + off * 16 + 4)) << 32);
        const uint64_t hi = static_cast<uint64_t>(lwu(a + off * 16 + 8)) | (static_cast<uint64_t>(lwu(a + off * 16 + 12)) << 32);
        const uint32_t nloop = lo & 0x7fff, flg = (lo >> 58) & 3, nreg0 = (lo >> 60) & 0xf;
        const uint32_t nreg = nreg0 == 0 ? 16 : nreg0;
        if (flg != 0) break;
        for (uint32_t i = 0; i < nloop; ++i)
            for (uint32_t r = 0; r < nreg; ++r) {
                const uint32_t q = a + (off + 1 + i * nreg + r) * 16;
                if (((hi >> (4 * r)) & 0xf) != 0xe) continue;
                regs[static_cast<uint8_t>(lbu(q + 8))] = static_cast<uint64_t>(lwu(q)) | (static_cast<uint64_t>(lwu(q + 4)) << 32);
            }
        off += 1 + nloop * nreg;
        if ((lo >> 15) & 1) break;
    }
}

glm::mat4 world_view_proj() {  // as tfrag_render_update builds the world's
    const glm::mat4 view = render_view_matrix();
    glm::mat4 proj(0.0f);
    const float fov = glm::radians(60.0f), n = kRenderNear, f = kRenderFar;
    proj[0][0] = render_proj_x(g_render_aspect, fov);
    proj[1][1] = render_proj_y(fov);
    proj[2][2] = -(f + n) / (f - n);
    proj[3][2] = -(2.0f * f * n) / (f - n);
    proj[2][3] = -1.0f;
    return proj * view;
}

// orig: (host) the model -> world matrix of a part's VU1 rows: the rows are the moby's model axes (x its
// scale) and its translation ((pos - camera) x 1024, w 1) through the draw loop's view (0x166cc0..0x166cf0,
// vf28..vf31: p = (u, c) V); (u, c) = p V^-1 gives the world point camera + u / (1024 c): homogeneous,
// linear in the model point
struct ViewInverse {
    bool ok = false;
    glm::dmat4 inv{1.0};
    glm::dvec3 cam{0.0};
};
ViewInverse view_inverse(const MrpFrame& f) {
    // the loop's transform (vf28..vf31 the view rows): p = x vf28 + y vf29 + z vf30 + w vf31 = A (x, y, z, w),
    // the rows as A's columns
    ViewInverse v;
    glm::dmat4 A;
    const MrpVec4* vr[4] = {&f.view_rows[0], &f.view_rows[1], &f.view_rows[2], &f.view_trans};
    for (int i = 0; i < 4; ++i) A[i] = glm::dvec4(vr[i]->x, vr[i]->y, vr[i]->z, vr[i]->w);
    v.ok = std::fabs(glm::determinant(A)) > 1e-30;
    if (v.ok) v.inv = glm::inverse(A);
    v.cam = glm::dvec3(f.cam_pos.x, f.cam_pos.y, f.cam_pos.z);
    return v;
}
bool world_of_rows(const ViewInverse& v, const MrpMat4& r, glm::dmat4& out) {
    if (!v.ok) return false;
    for (int i = 0; i < 4; ++i) {
        const glm::dvec4 p(r.r[i].x, r.r[i].y, r.r[i].z, r.r[i].w);
        const glm::dvec4 uc = v.inv * p;  // (u, c): the row's world offset x 1024 and its w
        const glm::dvec3 u(uc.x, uc.y, uc.z);
        out[i] = glm::dvec4(u / 1024.0 + v.cam * uc.w, uc.w);
    }
    return true;
}
// orig: (host) world_of_rows the other way: the rows the draw loop would send for a world matrix (the free hoverboard's)
void rows_of_world(const MrpFrame& f, const glm::dmat4& W, MrpMat4& r) {
    glm::dmat4 A;
    const MrpVec4* vr[4] = {&f.view_rows[0], &f.view_rows[1], &f.view_rows[2], &f.view_trans};
    for (int i = 0; i < 4; ++i) A[i] = glm::dvec4(vr[i]->x, vr[i]->y, vr[i]->z, vr[i]->w);
    const glm::dvec3 cam(f.cam_pos.x, f.cam_pos.y, f.cam_pos.z);
    for (int i = 0; i < 4; ++i) {
        const glm::dvec4 p = A * glm::dvec4((glm::dvec3(W[i]) - cam * W[i].w) * 1024.0, W[i].w);
        r.r[i] = MrpVec4{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z), static_cast<float>(p.w)};
    }
}

}  // namespace

// orig: (host) see mrp_moby_renderer.h
bool mrp_moby_active() { return g_frame.active; }

// orig: (host) see mrp_moby_renderer.h
void mrp_world_view_proj(float out[16]) { std::memcpy(out, glm::value_ptr(world_view_proj()), 16 * sizeof(float)); }

// orig: (host) see mrp_moby_renderer.h
bool mrp_moby_ready() {
    return g_moby_renderer == MobyRenderer::kMrp && (vk::is_active() ? g_vk_moby.inited : g_prog != 0) && g_level.loaded && g_level.id == moby_level_id();
}

// orig: (host) see mrp_moby_renderer.h
void mrp_moby_begin_frame(bool title, bool menu_mode, bool title_menu) {
    g_frame.title = title;
    g_frame.menu = menu_mode;
    g_frame.title_menu = title_menu && !menu_mode;
    g_frame.built = false;
    g_frame.uploaded = false;
    g_frame.active = false;
    g_prog_marks.clear();
    if (g_moby_renderer != MobyRenderer::kMrp || title) return;
    if (vk::is_active()) {
        if (!init_vk()) return;
    } else {
        if (!init_gl()) return;
    }
    const int id = moby_level_id();
    const int guest = g_mrp_guest_class.load();  // (the free hoverboard's class asked for after the level's load: loaded again with it)
    const bool guest_new = guest >= 0 && guest != g_level.guest_key && guest / 10000 != id;
    if (g_level.id != id || (!g_level.missing && ((vk::is_active() && !g_level.vk_loaded) || (!vk::is_active() && !g_level.gl_loaded))) || guest_new) load_level(id);
    // the level's loaded classes (0x197300 / 0x197a00, count 0x15ffc0): a new set (the level entered
    // again, a class loaded) forgets the classes resolved by header address
    uint64_t h = 1469598103934665603ull;
    const uint32_t n = std::min<uint32_t>(lwu(OV(0x0015ffc0)), 0x100);
    auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mix(n);
    for (uint32_t cb = 0; cb < n; ++cb) {
        mix(lwu(OV(0x00197300) + cb * 4));
        mix(lhu(OV(0x00197a00) + cb * 2));
    }
    if (g_level.loaded && h != g_level.table_hash) {
        g_level.table_hash = h;
        g_level.by_ptr.clear();
        g_level.miss.clear();
        g_level.guest_gpu.reset();
        g_level.verts.clear();
        g_level.indices.clear();
        g_level.dirty = true;
        g_level.vk_dirty = true;
    }
    g_frame.active = g_level.loaded;
}

namespace {

// orig: (host) the frame's draw list from the scene (mrp_scene_draw(): the real pass's, or its shadow's): parts, draw records, batches
// A drawn part's transforms, computed before build_frame's loop (on the job pool): the part's world matrix (from the rows the
// draw loop sent, or the moby's own transform when the view is singular: rows_ok false), its clip matrix, its lights (the key
// light turned with the level's sun), its position and radius
struct PartPre {
    glm::dmat4 W{1.0};
    bool rows_ok = false;
    float clip[16] = {}, light[32] = {};
    glm::vec3 pos{0.0f};
    float rad = 0.0f;
};
std::vector<PartPre> g_part_pre;            // (kept from frame to frame: their room reused)
std::vector<uint32_t> g_part_pre_first;     // a moby's first slot in g_part_pre (one slot a part, drawn or not)
std::vector<ClassGpu*> g_frame_classes;     // the frame's mobys' classes (class_of)

// orig: (host) one part's PartPre (no state of its own: any thread)
void part_pre(const MrpFrame& f, const MrpMoby& m, const ClassGpu* cg, const MrpPart& part, const ViewInverse& vinv, const glm::dmat4& vp, bool turned,
              const glm::mat3& turn, PartPre& out) {
    glm::dmat4 W(1.0);
    out.rows_ok = world_of_rows(vinv, part.model_to_view, W);
    if (!out.rows_ok) {  // (a singular view: the moby's own transform, 0x2559c0's inputs)
        for (int i = 0; i < 3; ++i)
            W[i] = glm::dvec4(m.rot[i].x * m.scale / 1024.0, m.rot[i].y * m.scale / 1024.0, m.rot[i].z * m.scale / 1024.0, 0.0);
        W[3] = glm::dvec4(m.pos[0], m.pos[1], m.pos[2], 1.0);
    }
    out.W = W;
    const glm::mat4 clip = glm::mat4(vp * W);
    std::memcpy(out.clip, glm::value_ptr(clip), sizeof out.clip);
    for (uint32_t q = 0; q < 8; ++q) std::memcpy(&out.light[q * 4], &f.records[m.record_first + 0x40 + q * 0x10], 16);
    if (turned) {  // the level's own sun (the modern lighting): the record's key light (its first direction, in the part's frame) turned with it
        glm::dmat3 L3(W);
        for (int i = 0; i < 3; ++i) L3[i] = glm::normalize(L3[i]);
        const glm::dvec3 kd(out.light[0], out.light[4], out.light[8]);
        glm::dvec3 t = glm::inverse(L3) * (glm::dmat3(turn) * (L3 * kd));
        if (glm::length(t) > 0.0) t *= glm::length(kd) / glm::length(t);
        out.light[0] = static_cast<float>(t.x);
        out.light[4] = static_cast<float>(t.y);
        out.light[8] = static_cast<float>(t.z);
    }
    out.pos = glm::vec3(W[3]) / static_cast<float>(W[3].w != 0.0 ? W[3].w : 1.0);
    out.rad = part_radius(cg, W);
}

// the emissive surfaces of the moby classes (mrp_lighting.h EmissiveSurface): (class, local texture) -> shader slot, made again when the
// list or the preview changes; the slot goes into the draw's texture word, bits 27..31, while the modern lighting is on
struct MobyEmissive {
    uint32_t gen = 0;
    std::unordered_map<uint32_t, uint8_t> slot;
} g_memis;
uint32_t emis_key(int o_class, int local) { return (static_cast<uint32_t>(static_cast<uint16_t>(o_class)) << 8) | (static_cast<uint32_t>(local) & 0xffu); }
// orig: (host) the table brought up to date (the frame's build)
void moby_emissive_refresh() {
    std::vector<EmissiveSurface> list;
    EmissiveSurface pv;
    bool has_pv = false;
    const uint32_t gen = mrp_emissive_snapshot(list, &pv, &has_pv);
    if (gen == g_memis.gen) return;
    g_memis.gen = gen;
    g_memis.slot.clear();
    for (size_t i = 0; i < list.size() && i < static_cast<size_t>(kEmissiveSlots); ++i)
        if (list[i].kind == 3) {
            g_memis.slot[emis_key(list[i].o_class, list[i].tex)] = static_cast<uint8_t>(i + 1);
            static const bool trace = std::getenv("RAC1_LIGHTS_TRACE") != nullptr;
            if (trace)
                std::printf("[lights] emissive moby %d tex %d: slot %zu (the class's local textures in use: mask %08x)\n", list[i].o_class, list[i].tex, i + 1,
                            mrp_moby_class_local_textures(list[i].o_class));
        }
    if (has_pv && pv.kind == 3) g_memis.slot[emis_key(pv.o_class, pv.tex)] = static_cast<uint8_t>(kEmissivePreviewSlot);
}

// orig: (host) the free hoverboard (g_mrp_guest_on, dev_hoverboard.cpp): the guest class (Level::guest) drawn on the hero as the
// frame drew him: his world matrix (his first part's rows) with the class's own scale, lifted g_mrp_guest_lift along his up
// axis, its joints at rest (as the game posed Kalebo III's board standing still), lit by the hero's lights; its near mesh's
// packets drawn after the frame's mobys as one more moby
void append_guest(Frame& F, const MrpFrame& f, const ViewInverse& vinv, const glm::dmat4& vp, bool turned, const glm::mat3& turn) {
    Level& L = g_level;
    if (L.guest == nullptr || F.menu || F.title) return;
    const uint32_t hero = lwu(0x0013f350u + 0x2080u);
    const MrpMoby* h = nullptr;
    for (const MrpMoby& m : f.mobys)
        if (m.moby == hero && !m.rt && !m.other_view && m.part_count > 0 && m.scale > 0.0f) {
            h = &m;
            break;
        }
    if (h == nullptr) return;
    glm::dmat4 Wh(1.0);
    if (!world_of_rows(vinv, f.parts[h->part_first].model_to_view, Wh)) return;
    if (L.guest_gpu == nullptr) L.guest_gpu = build_class(std::make_unique<mrp::MobyClass>(*L.guest), 0);
    ClassGpu* cg = L.guest_gpu.get();
    const mrp::MobyClass& c = *cg->asset;
    const double k = static_cast<double>(c.scale) / static_cast<double>(h->scale);
    glm::dmat4 W = Wh;
    for (int i = 0; i < 3; ++i) W[i] *= k;
    const glm::dvec3 up = glm::length(glm::dvec3(Wh[2])) > 0.0 ? glm::normalize(glm::dvec3(Wh[2])) : glm::dvec3(0.0, 0.0, 1.0);
    W[3] += glm::dvec4(up * static_cast<double>(g_mrp_guest_lift.load()), 0.0);
    MrpPart part = f.parts[h->part_first];
    rows_of_world(f, W, part.model_to_view);
    PartPre pre;
    part_pre(f, *h, cg, part, vinv, vp, turned, turn, pre);
    const uint32_t jbase = static_cast<uint32_t>(F.joints_all.size());
    const uint32_t nj = std::max<uint32_t>(1, c.joint_count);
    MrpMat4 rest;
    rest.r[0] = {1, 0, 0, 0};
    rest.r[1] = {0, 1, 0, 0};
    rest.r[2] = {0, 0, 1, 0};
    rest.r[3] = {0, 0, 0.333f, 1};  // (Kalebo III's board standing: every joint the identity, raised 0.333)
    F.joints_all.insert(F.joints_all.end(), nj, rest);
    PrimInfo cur_tex;
    cur_tex.word = 1;
    for (size_t pk = 0; pk < c.packets.size() && pk < cg->packets.size(); ++pk) {
        if (c.packets[pk].lod != 0) continue;
        const PacketGpu& pg = cg->packets[pk];
        PartGpu P{};
        for (int r = 0; r < 4; ++r) {
            P.rows[r * 4 + 0] = part.model_to_view.r[r].x;
            P.rows[r * 4 + 1] = part.model_to_view.r[r].y;
            P.rows[r * 4 + 2] = part.model_to_view.r[r].z;
            P.rows[r * 4 + 3] = part.model_to_view.r[r].w;
        }
        std::memcpy(P.clip, pre.clip, sizeof P.clip);
        std::memcpy(P.light, pre.light, sizeof P.light);
        P.info[0] = static_cast<int32_t>(jbase);
        P.info[1] = static_cast<int32_t>(nj);
        P.info[2] = 0;
        P.info[3] = part.mscal == 0x00a ? 1 : 0;
        const uint32_t part_index = static_cast<uint32_t>(F.parts.size());
        F.parts.push_back(P);
        F.part_pos.push_back(pre.pos);
        F.part_rad.push_back(pre.rad);
        F.part_own.push_back(0);
        for (const Run& run : pg.runs) {
            PrimInfo pi = cur_tex;
            if (run.prim >= 0 && static_cast<size_t>(run.prim) < pg.prims.size()) pi = pg.prims[static_cast<size_t>(run.prim)];
            const uint32_t ci = static_cast<uint32_t>(F.cmds.size() / 4);
            F.cmds.insert(F.cmds.end(), {part_index, pi.word, pi.layer, 0u});
            Key key;
            key.bucket = pi.bucket;
            key.sampler = pi.sampler;
            key.prim = F.prim;
            key.test = F.env_test;
            key.alpha = F.env_alpha;
            key.zbuf = F.env_zbuf;
            Batch* b = F.batches.empty() ? nullptr : &F.batches.back();
            if (b != nullptr && (pi.word & 1) != 0) {
                key.bucket = b->key.bucket;
                key.sampler = b->key.sampler;
            }
            if (b == nullptr || !(b->key == key)) {
                F.batches.push_back(Batch{key, static_cast<uint32_t>(F.draws.size()), 0});
                b = &F.batches.back();
            }
            F.draws.push_back(DrawCmd{run.count, 1, run.first_index, 0, ci});
            ++b->count;
        }
        if (pg.last_prim >= 0 && static_cast<size_t>(pg.last_prim) < pg.prims.size()) cur_tex = pg.prims[static_cast<size_t>(pg.last_prim)];
    }
}

void build_frame() {
    Frame& F = g_frame;
    F.built = true;
    moby_emissive_refresh();
    const bool emis = !g_memis.slot.empty() && g_modern_shadows != 0;  // (RAC1_EMISSIVE=0: the shaders ignore the emissive slots, uEmis[0])
    F.parts.clear();
    F.part_pos.clear();
    F.part_rad.clear();
    F.part_own.clear();
    F.cmds.clear();
    F.draws.clear();
    F.batches.clear();
    F.refs.clear();
    F.xf.clear();
    F.rt_views.clear();
    const MrpFrame& f = mrp_scene_draw();
    for (uint32_t i = 0; i < 0x200; ++i) F.normals[i] = lwc1(OV(0x00166080) + 4 * i);  // the normal table (the overlay's 0x166080: 256 x 2 floats)
    F.joints_all.assign(f.joints.begin(), f.joints.end());
    F.mode = F.menu ? 1 : g_mrp_moby_projection;
    F.fog_min = f.proj[0].x;
    F.proj = f.proj[1].x;
    F.offset[0] = f.proj[5].x;
    F.offset[1] = f.proj[5].y;
    F.offset[2] = f.proj[5].z;
    F.offset[3] = f.proj[5].w;
    {  // B+292: the PRIM GIF tag (PRE bit 46, PRIM bits 47..57, the y lane's bits 14 / 15..25)
        uint32_t y;
        std::memcpy(&y, &f.proj[1].y, 4);
        F.prim = (y >> 15) & 0x7ff;
    }
    {  // the frame's draw environment (0x13cf10, 11 qwords, render_passes.cpp): TEST_1, ALPHA_1, ZBUF_1, XYOFFSET_1
        std::map<uint8_t, uint64_t> env;
        gif_ad(0x0013cf10, 0xb, env);
        if (env.count(0x47)) F.env_test = env[0x47];
        if (env.count(0x42)) F.env_alpha = env[0x42];
        if (env.count(0x4e)) F.env_zbuf = env[0x4e];
        if (env.count(0x18)) F.env_xyoff = env[0x18];
    }
    const glm::dmat4 vp = glm::dmat4(world_view_proj());
    const ViewInverse vinv = view_inverse(f);
    glm::mat3 turn(1.0f);  // (the modern lighting's sun turned by the level's file: the mobys' key light with it)
    const bool turned = mrp_lighting_sun_turn(glm::value_ptr(turn));
    PrimInfo cur_tex;  // the GS's texture state along the chain (before the first switch: the white texture)
    cur_tex.word = 1;
    F.joints_extra.clear();
    // the casters' cache, with the lighting on in the world's projection
    static const bool cache_off = std::getenv("RAC1_SHADOW_MCACHE") != nullptr && std::atoi(std::getenv("RAC1_SHADOW_MCACHE")) == 0;  // (testing)
    const bool caching = g_modern_shadows != 0 && F.mode == 0 && !F.menu && !F.title && !cache_off;
    ++g_mcache_frame;
    if (!caching || g_mcache_level != static_cast<uint32_t>(moby_level_id())) {
        g_mcache.clear();
        g_mcache_level = moby_level_id();
    }
    // the mobys' classes (in the mobys' order, on this thread: a class's first use loads it to the GPU), then each drawn part's
    // transforms (its world matrix, clip matrix, lights, position, radius: pure arithmetic on the scene) on the job pool, a
    // chunk of mobys per job, each part into its own slot; the loop below takes them in order: the same lists as one moby
    // after another (RAC1_JOBS=1), on any number of threads
    static const int hide_class = std::getenv("RAC1_MRP_HIDE") != nullptr ? std::atoi(std::getenv("RAC1_MRP_HIDE")) : -9999;  // (testing: a class not drawn)
    g_frame_classes.resize(f.mobys.size());
    g_part_pre_first.resize(f.mobys.size() + 1);
    {
        uint32_t slots = 0;
        for (uint32_t mi = 0; mi < f.mobys.size(); ++mi) {
            const MrpMoby& m = f.mobys[mi];
            g_frame_classes[mi] = class_of(m.class_ptr, m.class_id);
            g_part_pre_first[mi] = slots;
            if (g_frame_classes[mi] != nullptr && m.class_id != hide_class) slots += std::min(m.part_count, m.packet_count);
        }
        g_part_pre_first[f.mobys.size()] = slots;
        if (g_part_pre.size() < slots) g_part_pre.resize(slots);
    }
    jobs::parallel_for(f.mobys.size(), 8, [&](size_t b, size_t e) {
        for (size_t mi = b; mi < e; ++mi) {
            const MrpMoby& m = f.mobys[mi];
            const ClassGpu* cg = g_frame_classes[mi];
            if (cg == nullptr || m.class_id == hide_class) continue;
            const uint32_t nparts = std::min(m.part_count, m.packet_count);
            for (uint32_t k = 0; k < nparts; ++k) {
                const uint32_t pk = m.first_packet + k;
                if (pk >= cg->packets.size() || cg->asset->packets[pk].lod == 2) continue;
                part_pre(f, m, cg, f.parts[m.part_first + k], vinv, vp, turned, turn, g_part_pre[g_part_pre_first[mi] + k]);
            }
        }
    });
    for (uint32_t mi = 0; mi < f.mobys.size(); ++mi) {
        const MrpMoby& m = f.mobys[mi];
        const bool own_view = F.title_menu && m.other_view && !m.rt;
        CachedMoby* cm = nullptr;
        size_t cm_used = 0;  // (the moby's cached parts kept as they are, each one's room reused: no allocations frame after frame)
        struct Trim {
            CachedMoby*& cm;
            size_t& used;
            ~Trim() {
                if (cm != nullptr) cm->parts.resize(used);
            }
        } trim{cm, cm_used};
        if (caching && !own_view) {
            cm = &g_mcache[m.moby];
            cm->class_id = m.class_id;
            std::memcpy(cm->pos, m.pos, sizeof cm->pos);
            cm->seen = g_mcache_frame;
            cm->joints.clear();
            for (uint32_t j = 0; j < m.joint_count && m.joint_first + j < f.joints.size(); ++j) cm->joints.push_back(f.joints[m.joint_first + j]);
        }
        ClassGpu* cg = g_frame_classes[mi];
        if (cg == nullptr) {
            ++g_st.no_asset_mobys;
            continue;
        }
        static const bool class_check = std::getenv("RAC1_MRP_CLASS_CHECK") != nullptr;  // (testing: the class the MRP draws vs the game's)
        if (class_check) {
            ++g_st.cc_mobys;
            const uint32_t pt = lwu(m.class_ptr), vd = pt != 0 && pt < 0x02000000u ? lwu(pt + 8) : 0, vn = pt != 0 && pt < 0x02000000u ? lbu(pt + 0xc) * 16u : 0;
            const auto& v0 = cg->asset->packets.empty() ? std::vector<uint8_t>{} : cg->asset->packets[0].vdata;
            bool same = vd != 0 && vn == v0.size();
            for (size_t i = 0; same && i < v0.size(); ++i) same = lbu(vd + static_cast<uint32_t>(i)) == v0[i];
            const bool id_ok = cg->asset->o_class == m.class_id || m.class_id == 0 || cg->asset->o_class < 0;  // (an armor-matched asset: by its vertex data)
            if (!same || !id_ok) {
                ++g_st.cc_bad;
                static int shown = 0;
                if (shown++ < 20)
                    std::printf("[mrp-class] moby %08x class %d header %08x: asset o_class %d, first packet's vertex data %s (EE %u bytes at %08x, asset %zu)\n", m.moby,
                                m.class_id, m.class_ptr, cg->asset->o_class, same ? "equal" : "DIFFERS", vn, vd, v0.size());
            if (!same || !id_ok) {
                static int shown_t = 0;
                if (shown_t++ < 3) {
                    std::printf("[mrp-class]  the class table (%u):", lwu(OV(0x0015ffc0)));
                    for (uint32_t cb = 0; cb < lwu(OV(0x0015ffc0)) && cb < 0x100; ++cb)
                        if (lwu(OV(0x00197300) + cb * 4) == m.class_ptr || lhu(OV(0x00197a00) + cb * 2) == static_cast<uint32_t>(m.class_id) ||
                            lhu(OV(0x00197a00) + cb * 2) == static_cast<uint32_t>(cg->asset->o_class))
                            std::printf(" [%u] %08x id %u", cb, lwu(OV(0x00197300) + cb * 4), lhu(OV(0x00197a00) + cb * 2));
                    std::printf("\n");
                }
            }
            }
        }
        if (m.class_id == hide_class) continue;
        ++g_st.mobys;
        const mrp::MobyClass& c = *cg->asset;
        if (m.rt) {  // its own view: the block the draw loop sent at its call
            Frame::RtView& rv = F.rt_views[m.moby];
            rv.fog_min = m.rt_proj[0].x;
            rv.proj = m.rt_proj[1].x;
            rv.offset[0] = m.rt_proj[5].x;
            rv.offset[1] = m.rt_proj[5].y;
            rv.offset[2] = m.rt_proj[5].z;
            rv.offset[3] = m.rt_proj[5].w;
        }
        // the moby's GS state: the frame's, or the one its first part's DIRECT sets (the fade / alpha test, 0x2559c0)
        uint64_t test = F.env_test, alpha = F.env_alpha;
        if (m.part_count > 0 && f.parts[m.part_first].gs_resolved) {  // (a shadow frame's: read at its capture)
            if (f.parts[m.part_first].has_test) test = f.parts[m.part_first].test;
            if (f.parts[m.part_first].has_alpha) alpha = f.parts[m.part_first].alpha;
        } else if (m.part_count > 0 && f.parts[m.part_first].has_gs_state) {
            std::map<uint8_t, uint64_t> regs;
            gif_ad(f.parts[m.part_first].direct_data, f.parts[m.part_first].direct_qwc, regs);
            if (regs.count(0x47)) test = regs[0x47];
            if (regs.count(0x42)) alpha = regs[0x42];
        }
        // the sub-mesh colour (0x255870 over the chain, the record's +0xbc alpha: mrp_scene_asset)
        float sub[4] = {static_cast<float>(lbu(m.moby + 0x90)), static_cast<float>(lbu(m.moby + 0x91)), static_cast<float>(lbu(m.moby + 0x92)),
                        static_cast<float>(f.records[m.record_first + ee_byte_addr(0xbc)])};  // (the record holds host-order words: its PS2 byte +0xbc)
        const uint32_t nparts = std::min(m.part_count, m.packet_count);
        static const uint32_t trace_moby = std::getenv("RAC1_MRP_TRACE_MOBY") != nullptr ? static_cast<uint32_t>(std::strtoul(std::getenv("RAC1_MRP_TRACE_MOBY"), nullptr, 0)) : 0;  // (testing)
        if (trace_moby == 1 && m.part_count > 0 && f.parts[m.part_first].has_gs_state) {  // (testing: every moby's own GS registers)
            std::map<uint8_t, uint64_t> regs;
            gif_ad(f.parts[m.part_first].direct_data, f.parts[m.part_first].direct_qwc, regs);
            std::printf("[mrp-trace] class %d moby %08x flags %04x tint %u %u %u %u regs:", m.class_id, m.moby, m.flags, m.tint[0], m.tint[1], m.tint[2], m.tint[3]);
            for (const auto& [r, v] : regs) std::printf(" %02x=%llx", r, static_cast<unsigned long long>(v));
            std::printf("\n");
        }
        if (trace_moby != 0 && m.moby == trace_moby) {
            std::printf("[mrp-trace] moby %08x class %d parts %u packets %u first %u test %llx alpha %llx sub %g %g %g %g tint %u %u %u %u flags %04x deferred %d far %d", m.moby, m.class_id,
                        m.part_count, m.packet_count, m.first_packet, static_cast<unsigned long long>(test), static_cast<unsigned long long>(alpha), sub[0], sub[1], sub[2], sub[3],
                        m.tint[0], m.tint[1], m.tint[2], m.tint[3], m.flags, m.deferred ? 1 : 0, m.far_lod ? 1 : 0);
            for (uint32_t k = 0; k < nparts && k < 4; ++k) {
                const uint32_t pk = m.first_packet + k;
                if (pk < cg->packets.size()) std::printf(" [pk %u lod %d m2v.w %g %g %g %g]", pk, c.packets[pk].lod, f.parts[m.part_first + k].model_to_view.r[3].x,
                                                         f.parts[m.part_first + k].model_to_view.r[3].y, f.parts[m.part_first + k].model_to_view.r[3].z, f.parts[m.part_first + k].model_to_view.r[3].w);
            }
            std::printf("\n");
        }
        if (m.part_count != m.packet_count) {
            if (m.part_count > m.packet_count) g_st.shadow_parts += m.part_count - m.packet_count;
            else ++g_st.part_mismatch;
        }
        for (uint32_t k = 0; k < nparts; ++k) {
            const uint32_t pk = m.first_packet + k;
            if (pk >= cg->packets.size() || c.packets[pk].lod == 2) continue;
            const PacketGpu& pg = cg->packets[pk];
            const MrpPart& part = f.parts[m.part_first + k];
            PartGpu P{};
            for (int r = 0; r < 4; ++r) {
                P.rows[r * 4 + 0] = part.model_to_view.r[r].x;
                P.rows[r * 4 + 1] = part.model_to_view.r[r].y;
                P.rows[r * 4 + 2] = part.model_to_view.r[r].z;
                P.rows[r * 4 + 3] = part.model_to_view.r[r].w;
            }
            const PartPre& pre = g_part_pre[g_part_pre_first[mi] + k];
            const glm::dmat4& W = pre.W;
            if (pre.rows_ok && g_env_debug) {  // the rows' world point against the moby's own transform
                const glm::dvec4 o = W[3];
                const double e = glm::length(glm::dvec3(o) / o.w - glm::dvec3(m.pos[0], m.pos[1], m.pos[2]));
                g_st.world_err_max = std::max(g_st.world_err_max, e);
                static int shown_w = 0;
                if (e > 0.01 && shown_w++ < 8)
                    std::printf("[mrp] world origin of class %d part %u: %.4f %.4f %.4f, the moby at %.4f %.4f %.4f (flags %04x, shadow mesh %d, deferred %d)\n", m.class_id, k,
                                o.x / o.w, o.y / o.w, o.z / o.w, m.pos[0], m.pos[1], m.pos[2], m.flags, m.shadow_mesh ? 1 : 0, m.deferred ? 1 : 0);
            }
            std::memcpy(P.clip, pre.clip, sizeof P.clip);
            std::memcpy(P.light, pre.light, sizeof P.light);
            const bool tint = (m.flags & 0x10) != 0 && pg.tint_part;
            std::memcpy(P.sub, sub, sizeof sub);
            P.info[0] = static_cast<int32_t>(m.joint_first);
            P.info[1] = static_cast<int32_t>(m.joint_count);
            P.info[2] = tint ? 1 : 0;
            P.info[3] = part.mscal == 0x00a ? 1 : 0;  // the clipping program: the fog per vertex
            const uint32_t part_index = static_cast<uint32_t>(F.parts.size());
            F.parts.push_back(P);
            F.part_pos.push_back(pre.pos);
            F.part_rad.push_back(pre.rad);
            F.part_own.push_back(own_view ? 1 : 0);
            F.refs.push_back({mi, cg, pk, part_index, tint, test, alpha});
            if (!own_view && !m.rt && (F.xf.empty() || F.xf.back().moby != m.moby)) {  // (the moby's first part: its frame for the light editor)
                Frame::MobyXf x;
                x.moby = m.moby;
                x.class_id = m.class_id;
                x.W = glm::mat4(W);
                x.joint_first = m.joint_first;
                x.joint_count = m.joint_count;
                const auto& b = c.bsphere;
                const glm::dvec4 cw = W * glm::dvec4(b[0], b[1], b[2], 1.0);
                x.centre = glm::vec3(cw) / static_cast<float>(cw.w != 0.0 ? cw.w : 1.0);
                const float sc = static_cast<float>(std::max({glm::length(glm::dvec3(W[0])), glm::length(glm::dvec3(W[1])), glm::length(glm::dvec3(W[2]))}));
                x.radius = std::isfinite(sc * std::fabs(b[3])) ? sc * std::fabs(b[3]) : 0.0f;
                F.xf.push_back(x);
            }
            CachedPart* cp = nullptr;
            if (cm != nullptr) {
                if (cm_used < cm->parts.size()) {
                    cp = &cm->parts[cm_used];
                    cp->runs.clear();
                } else {
                    cp = &cm->parts.emplace_back();
                }
                ++cm_used;
                cp->W = W;
                cp->rad = F.part_rad.back();
                std::memcpy(cp->rows, P.rows, sizeof P.rows);
                std::memcpy(cp->light, P.light, sizeof P.light);
                std::memcpy(cp->sub, P.sub, sizeof P.sub);
                cp->info2 = P.info[2];
                cp->info3 = P.info[3];
            }
            ++g_st.parts;
            for (const Run& run : pg.runs) {
                // the triangles before the packet's first texture switch: the texture the GS has from the
                // packets drawn before (the kick program's A+D comes after its first vertices, e.g. Ratchet's packet 1)
                PrimInfo pi = cur_tex;
                if (run.prim >= 0 && static_cast<size_t>(run.prim) < pg.prims.size()) pi = pg.prims[static_cast<size_t>(run.prim)];
                const uint32_t ci = static_cast<uint32_t>(F.cmds.size() / 4);
                uint32_t word = pi.word;
                // (testing, RAC1_MRP_MIP0=1: the textures whose big mip the frame did not upload, 0x199e80's slot 0: the
                // faithful path samples stale GS memory where the GS's level is 0; shown magenta there)
                static const bool mark_mip0 = std::getenv("RAC1_MRP_MIP0") != nullptr;
                if (mark_mip0 && pi.game_texture >= 0 && lh(OV(0x00199e80) + 4 * static_cast<uint32_t>(pi.game_texture)) == 0) word |= 1u << 25;
                if (mark_mip0 && pi.game_texture >= 0 && lh(OV(0x00199e80) + 4 * static_cast<uint32_t>(pi.game_texture) + 2) == 0) word |= 1u << 26;
                if (emis && pi.local >= 0)  // (an emissive surface's slot)
                    if (const auto es = g_memis.slot.find(emis_key(m.class_id, pi.local)); es != g_memis.slot.end()) word |= static_cast<uint32_t>(es->second) << 27;
                F.cmds.insert(F.cmds.end(), {part_index, word, pi.layer, 0u});
                Key key;
                key.bucket = pi.bucket;
                key.sampler = pi.sampler;
                key.prim = F.prim;
                key.test = test;
                key.alpha = alpha;
                key.zbuf = F.env_zbuf;
                key.deferred = m.deferred;
                key.rt = m.rt ? m.moby : 0u;
                key.own_view = own_view;
                Batch* b = F.batches.empty() ? nullptr : &F.batches.back();
                if (b != nullptr && (pi.word & 1) != 0) {  // the white texture: any array, any sampler
                    key.bucket = b->key.bucket;
                    key.sampler = b->key.sampler;
                }
                if (b == nullptr || !(b->key == key)) {
                    F.batches.push_back(Batch{key, static_cast<uint32_t>(F.draws.size()), 0});
                    b = &F.batches.back();
                }
                F.draws.push_back(DrawCmd{run.count, 1, run.first_index, 0, ci});
                ++b->count;
                ++g_st.draws;
                if (cp != nullptr) cp->runs.push_back({run.count, run.first_index, word, pi.layer, key});
            }
            if (pg.last_prim >= 0 && static_cast<size_t>(pg.last_prim) < pg.prims.size()) cur_tex = pg.prims[static_cast<size_t>(pg.last_prim)];
        }
        // the parts after the class's packets: the shadow mesh (class +6 / +7, 0x256b50), not drawn; the TEX0 it leaves
        for (uint32_t k = nparts; k < m.part_count; ++k)
            for (size_t q = 0; q < c.packets.size(); ++q)
                if (c.packets[q].lod == 2 && c.packets[q].index_in_lod == k - nparts) {
                    const PacketGpu& sg = cg->packets[q];
                    if (sg.last_prim >= 0 && static_cast<size_t>(sg.last_prim) < sg.prims.size()) cur_tex = sg.prims[static_cast<size_t>(sg.last_prim)];
                }
    }
    if (g_mrp_guest_on.load()) append_guest(F, f, vinv, vp, turned, turn);
    if (!caching) return;
    // the cached mobys the frame did not draw: kept while the moby is alive, of its class and still (the moby's EE state now;
    // a slot reused or a moby moved drops its copy), drawn as casters within reach of the shadows
    const glm::dvec3 cam = vinv.cam;
    for (auto it = g_mcache.begin(); it != g_mcache.end();) {
        CachedMoby& c = it->second;
        if (c.seen == g_mcache_frame) {
            ++it;
            continue;
        }
        const uint32_t mb = it->first;
        const int8_t state = static_cast<int8_t>(lbu(mb + 0x20));
        bool keep = state >= 0 && lh(mb + 0xa6) == c.class_id;
        for (int i = 0; keep && i < 3; ++i) keep = std::fabs(lwc1(mb + 0x10 + 4 * static_cast<uint32_t>(i)) - c.pos[i]) < 1e-3f;
        if (!keep) {
            it = g_mcache.erase(it);
            continue;
        }
        ++it;
        if (glm::length(glm::dvec3(c.pos[0], c.pos[1], c.pos[2]) - cam) > 400.0) continue;
        const uint32_t jbase = static_cast<uint32_t>(F.joints_all.size());
        F.joints_all.insert(F.joints_all.end(), c.joints.begin(), c.joints.end());
        F.joints_extra.insert(F.joints_extra.end(), c.joints.begin(), c.joints.end());
        for (const CachedPart& cp : c.parts) {
            PartGpu P{};
            const glm::mat4 clip = glm::mat4(vp * cp.W);
            std::memcpy(P.clip, glm::value_ptr(clip), sizeof P.clip);
            std::memcpy(P.rows, cp.rows, sizeof P.rows);
            std::memcpy(P.light, cp.light, sizeof P.light);
            std::memcpy(P.sub, cp.sub, sizeof P.sub);
            P.info[0] = static_cast<int32_t>(jbase);
            P.info[1] = static_cast<int32_t>(c.joints.size());
            P.info[2] = cp.info2;
            P.info[3] = cp.info3;
            const uint32_t part_index = static_cast<uint32_t>(F.parts.size());
            F.parts.push_back(P);
            F.part_pos.push_back(glm::vec3(cp.W[3]) / static_cast<float>(cp.W[3].w != 0.0 ? cp.W[3].w : 1.0));
            F.part_rad.push_back(cp.rad);
            F.part_own.push_back(0);
            for (const CachedRun& r : cp.runs) {
                const uint32_t ci = static_cast<uint32_t>(F.cmds.size() / 4);
                F.cmds.insert(F.cmds.end(), {part_index, r.word, r.layer, 0u});
                Key key = r.key;
                key.caster_only = true;
                key.deferred = false;
                if (F.batches.empty() || !(F.batches.back().key == key)) F.batches.push_back(Batch{key, static_cast<uint32_t>(F.draws.size()), 0});
                F.draws.push_back(DrawCmd{r.count, 1, r.first_index, 0, ci});
                ++F.batches.back().count;
            }
        }
    }
}

// orig: (host) the level's vertex / index data and the frame's buffers to the GPU
void upload_frame() {
    if (g_frame.uploaded) return;
    g_frame.uploaded = true;
    Level& L = g_level;

    if (vk::is_active()) {
        if (L.vk_dirty || L.vk_vbo.buffer == VK_NULL_HANDLE || L.vk_ibo.buffer == VK_NULL_HANDLE) {
            GpuVertex dummy_v{};
            uint32_t dummy_i = 0;
            const void* v_ptr = L.verts.empty() ? static_cast<const void*>(&dummy_v) : L.verts.data();
            const VkDeviceSize v_bytes = std::max<VkDeviceSize>(L.verts.size() * sizeof(GpuVertex), sizeof(GpuVertex));
            const void* i_ptr = L.indices.empty() ? static_cast<const void*>(&dummy_i) : L.indices.data();
            const VkDeviceSize i_bytes = std::max<VkDeviceSize>(L.indices.size() * sizeof(uint32_t), sizeof(uint32_t));
            vk::upload_buffer(L.vk_vbo, v_ptr, v_bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            vk::upload_buffer(L.vk_ibo, i_ptr, i_bytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            L.vk_dirty = false;
        }
        auto alloc_ssbo = [](const void* data, size_t bytes) {
            const VkDeviceSize sz = std::max<VkDeviceSize>(bytes, 16);
            vk::RingAlloc r = vk::alloc_ring(sz, 16);
            if (bytes > 0 && data != nullptr) std::memcpy(r.ptr, data, bytes);
            else std::memset(r.ptr, 0, sz);
            return r;
        };
        g_vk_moby.joints_alloc = alloc_ssbo(g_frame.joints_all.data(), g_frame.joints_all.size() * sizeof(MrpMat4));
        g_vk_moby.parts_alloc = alloc_ssbo(g_frame.parts.data(), g_frame.parts.size() * sizeof(PartGpu));
        g_vk_moby.cmds_alloc = alloc_ssbo(g_frame.cmds.data(), g_frame.cmds.size() * 4);
        g_vk_moby.normals_alloc = alloc_ssbo(g_frame.normals, sizeof(g_frame.normals));
        g_vk_moby.indirect_alloc = alloc_ssbo(g_frame.draws.data(), g_frame.draws.size() * sizeof(DrawCmd));
        return;
    }

    if (L.dirty) {
        glBindVertexArray(L.vao);
        glBindBuffer(GL_ARRAY_BUFFER, L.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(L.verts.size() * sizeof(GpuVertex)), L.verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, L.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(L.indices.size() * sizeof(uint32_t)), L.indices.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);
        L.dirty = false;
    }
    auto ssbo = [](GLuint b, int binding, const void* data, size_t bytes) {
        glBindBuffer(kGlShaderStorageBuffer, b);
        glBufferData(kGlShaderStorageBuffer, static_cast<GLsizeiptr>(std::max<size_t>(bytes, 16)), nullptr, GL_STREAM_DRAW);
        if (bytes > 0) glBufferSubData(kGlShaderStorageBuffer, 0, static_cast<GLsizeiptr>(bytes), data);
        glBindBufferBase(kGlShaderStorageBuffer, static_cast<GLuint>(binding), b);
    };
    ssbo(g_ssbo[0], 0, g_frame.joints_all.data(), g_frame.joints_all.size() * sizeof(MrpMat4));
    ssbo(g_ssbo[1], 1, g_frame.parts.data(), g_frame.parts.size() * sizeof(PartGpu));
    ssbo(g_ssbo[2], 2, g_frame.cmds.data(), g_frame.cmds.size() * 4);
    ssbo(g_ssbo[3], 3, g_frame.normals, sizeof g_frame.normals);
    glBindBuffer(kGlShaderStorageBuffer, 0);
    glBindBuffer(kGlDrawIndirectBuffer, g_indirect);
    glBufferData(kGlDrawIndirectBuffer, static_cast<GLsizeiptr>(std::max<size_t>(g_frame.draws.size() * sizeof(DrawCmd), 16)), g_frame.draws.data(), GL_STREAM_DRAW);
}

void set_common_uniforms(const Loc& l, int win_w, int win_h, const gs::Viewport* vp) {
    const Frame& F = g_frame;
    glUniform1i(l.mode, F.mode);
    glUniform1f(l.proj, F.proj);
    glUniform4f(l.offset, F.offset[0], F.offset[1], F.offset[2], F.offset[3]);
    float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
    if (fb_w <= 0.0f) fb_w = 512.0f;
    if (fb_h <= 0.0f) fb_h = 416.0f;
    const float sx = vp != nullptr ? (F.menu ? vp->xscale : vp->xscale_3d) : 1.0f;
    glUniform4f(l.frame, fb_w, fb_h, sx, 0.0f);
    double da = 0.0, db = 0.0;
    const bool has = render_gs_depth(da, db);
    float xo = static_cast<float>(F.env_xyoff & 0xffff), yo = static_cast<float>((F.env_xyoff >> 32) & 0xffff);
    if (F.env_xyoff == 0) {
        xo = (2048.0f - fb_w * 0.5f) * 16.0f;
        yo = (2048.0f - fb_h * 0.5f) * 16.0f;
    }
    glUniform4f(l.xyoff, xo, yo, has ? static_cast<float>(da) : 0.5f, has ? static_cast<float>(db) : 0.0f);
    glUniform1f(l.fogmin, F.fog_min);
    if (g_rt_draw != nullptr) {  // a render target: its view, its size (one pixel per GS pixel, rows not flipped: uFrame.w), its XYOFFSET, its depth
        const RtDraw& r = *g_rt_draw;
        glUniform1i(l.mode, 1);
        glUniform1f(l.proj, r.view->proj);
        glUniform4f(l.offset, r.view->offset[0], r.view->offset[1], r.view->offset[2], r.view->offset[3]);
        glUniform4f(l.frame, r.w, r.h, 1.0f, 1.0f);
        glUniform4f(l.xyoff, r.xo, r.yo, r.da, r.db);
        glUniform1f(l.fogmin, r.view->fog_min);
    }
    if (F.mode == 0 && g_rt_draw == nullptr) mrp_lighting_bind_receiver(l.sh);
    else mrp_lighting_bind_off(l.sh);
    {  // FOGCOL (the GS register the game's packets set: R, G, B in the low 24 bits)
        const uint64_t fc = gs::gs_global().reg(0x3d);
        glUniform3f(l.fogcol, static_cast<float>(fc & 0xff) / 255.0f, static_cast<float>((fc >> 8) & 0xff) / 255.0f, static_cast<float>((fc >> 16) & 0xff) / 255.0f);
    }
    (void)win_w;
    (void)win_h;
}

void fill_common_vk_push(MobyVkPush& push, const gs::Viewport* vp) {
    const Frame& F = g_frame;
    push.uMode = F.mode;
    push.uProj = F.proj;
    std::memcpy(push.uOffset, F.offset, sizeof(push.uOffset));
    float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
    if (fb_w <= 0.0f) fb_w = 512.0f;
    if (fb_h <= 0.0f) fb_h = 416.0f;
    const float sx = vp != nullptr ? (F.menu ? vp->xscale : vp->xscale_3d) : 1.0f;
    push.uFrame[0] = fb_w;
    push.uFrame[1] = fb_h;
    push.uFrame[2] = sx;
    push.uFrame[3] = 0.0f;
    double da = 0.0, db = 0.0;
    const bool has = render_gs_depth(da, db);
    float xo = static_cast<float>(F.env_xyoff & 0xffff), yo = static_cast<float>((F.env_xyoff >> 32) & 0xffff);
    if (F.env_xyoff == 0) {
        xo = (2048.0f - fb_w * 0.5f) * 16.0f;
        yo = (2048.0f - fb_h * 0.5f) * 16.0f;
    }
    push.uXyOff[0] = xo;
    push.uXyOff[1] = yo;
    push.uXyOff[2] = has ? static_cast<float>(da) : 0.5f;
    push.uXyOff[3] = has ? static_cast<float>(db) : 0.0f;
    const uint64_t fc = gs::gs_global().reg(0x3d);
    push.uFogCol_Min[0] = static_cast<float>(fc & 0xff) / 255.0f;
    push.uFogCol_Min[1] = static_cast<float>((fc >> 8) & 0xff) / 255.0f;
    push.uFogCol_Min[2] = static_cast<float>((fc >> 16) & 0xff) / 255.0f;
    push.uFogCol_Min[3] = F.fog_min;
    if (g_rt_draw != nullptr) {
        const RtDraw& r = *g_rt_draw;
        push.uMode = 1;
        push.uProj = r.view->proj;
        std::memcpy(push.uOffset, r.view->offset, sizeof(push.uOffset));
        push.uFrame[0] = r.w;
        push.uFrame[1] = r.h;
        push.uFrame[2] = 1.0f;
        push.uFrame[3] = 1.0f;
        push.uXyOff[0] = r.xo;
        push.uXyOff[1] = r.yo;
        push.uXyOff[2] = r.da;
        push.uXyOff[3] = r.db;
        push.uFogCol_Min[3] = r.view->fog_min;
    }
}

SplitScope::SplitScope() {
    if (!vk::is_active() && mrp_lighting_active() && g_prog_split != 0) {
        on = true;
        std::swap(g_prog, g_prog_split);
        std::swap(g_loc, g_loc_split);
    }
}
SplitScope::~SplitScope() {
    if (on) {
        std::swap(g_prog, g_prog_split);
        std::swap(g_loc, g_loc_split);
    }
}

// orig: (host) the draw itself (the bound framebuffer, the viewport set by the caller)
// `rt`: only the batches of the moby at that address drawn into a render target (0: the frame's own, those never)
void draw_batches(int win_w, int win_h, const gs::Viewport* vp, bool shadows = false, uint32_t rt = 0) {
    if (vk::is_active()) {
        if (!g_frame.uploaded) upload_frame();
        Frame& F = g_frame;
        MobyVkPush push{};
        fill_common_vk_push(push, vp);
        push.uMipMode = g_mrp_moby_mip;
        push.uShade = g_render_shade;
        push.uPass = 0;

        if (!g_mrp_moby_keep_scissor) vk::set_scissor(false);
        bool cur_depth_clamp = (F.mode == 1 || rt != 0);
        const int mm = g_mrp_moby_mip != 0 ? 1 : 0;
        const bool split_prog = mrp_lighting_active();
        const uint64_t pl = reinterpret_cast<uint64_t>(g_vk_moby.pipe_layout);
        bool light_off = !(F.mode == 0 && g_rt_draw == nullptr);
        auto bind_light = [&] {
            if (light_off) mrp_lighting_vk_bind_off(pl);
            else mrp_lighting_vk_bind_receiver(pl, false, split_prog);
        };

        vk::ensure_rendering();
        VkCommandBuffer cb = vk::cmd();
        VkDeviceSize voff = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &g_level.vk_vbo.buffer, &voff);
        vkCmdBindIndexBuffer(cb, g_level.vk_ibo.buffer, 0, VK_INDEX_TYPE_UINT32);
        bind_light();

        Key cur;
        bool have = false;
        bool shadows_done = !shadows || F.mode != 0 || g_modern_shadows != 0;
        auto draw_shadows = [&] {
            shadows_done = true;
            const glm::mat4 m = world_view_proj();
            if (!mrp_shadows_draw(glm::value_ptr(m))) return;
            vk::ensure_rendering();
            VkDeviceSize vo = 0;
            vkCmdBindVertexBuffers(cb, 0, 1, &g_level.vk_vbo.buffer, &vo);
            vkCmdBindIndexBuffer(cb, g_level.vk_ibo.buffer, 0, VK_INDEX_TYPE_UINT32);
            bind_light();
            have = false;
        };
        bool own_on = false;
        auto own_view = [&](bool on) {
            if (on == own_on) return;
            own_on = on;
            push.uMode = on ? 1 : F.mode;
            float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
            if (fb_w <= 0.0f) fb_w = 512.0f;
            if (fb_h <= 0.0f) fb_h = 416.0f;
            const float xs = vp != nullptr ? vp->xscale : 1.0f;
            push.uFrame[0] = fb_w;
            push.uFrame[1] = fb_h;
            push.uFrame[2] = on ? xs : (vp != nullptr ? vp->xscale_3d : 1.0f);
            push.uFrame[3] = 0.0f;
            if (on) light_off = true;
            else if (F.mode == 0 && g_rt_draw == nullptr) light_off = false;
            bind_light();
            cur_depth_clamp = (on || F.mode == 1);
            if (on && vp != nullptr && xs < 1.0f) {
                const int cw = static_cast<int>(static_cast<float>(vp->w) * xs);
                vk::set_scissor(true, vp->x + (vp->w - cw) / 2, vp->y, cw, vp->h);
            } else if (!g_mrp_moby_keep_scissor) {
                vk::set_scissor(false);
            }
        };

        const auto& rt_info = vk::current_target();
        const VkFormat cfmt = rt_info.color_view != VK_NULL_HANDLE ? rt_info.color_format : VK_FORMAT_UNDEFINED;
        const VkFormat dfmt = rt_info.depth_view != VK_NULL_HANDLE ? rt_info.depth_format : VK_FORMAT_UNDEFINED;
        const bool has_stencil = rt_info.stencil_view != VK_NULL_HANDLE;

        for (const Batch& b : F.batches) {
            const Key& k = b.key;
            if (k.caster_only) continue;
            if (k.rt != rt) continue;
            if (rt == 0) own_view(k.own_view);
            if (!shadows_done && k.deferred) draw_shadows();
            {
                const char* why = nullptr;
                if (static_cast<uint64_t>(b.first) + b.count > F.draws.size()) why = "draw commands";
                for (uint32_t i = b.first; why == nullptr && i < b.first + b.count; ++i) {
                    const DrawCmd& c = F.draws[i];
                    if (static_cast<uint64_t>(c.first_index) + c.count > g_level.indices.size()) why = "indices";
                    else if (c.base_vertex != 0 || c.instances != 1 || (static_cast<uint64_t>(c.base_instance) + 1) * 4 > F.cmds.size()) why = "draw records";
                }
                if (why != nullptr) {
                    static uint32_t shown = 0;
                    if (shown++ < 8)
                        std::fprintf(stderr, "[mrp] batch refused (%s): first %u count %u (%zu draw commands, %zu records, %zu indices)\n", why, b.first, b.count, F.draws.size(),
                                     F.cmds.size(), g_level.indices.size());
                    continue;
                }
            }
            if (!have || k.bucket != cur.bucket || k.sampler != cur.sampler) {
                bind_moby_vk_set0(cb, k.bucket, k.sampler, mm);
            }
            if (!have || k.prim != cur.prim || k.test != cur.test || k.alpha != cur.alpha || k.zbuf != cur.zbuf) {
                push.uTme = gs::field(k.prim, 4, 1) != 0 ? 1 : 0;
                push.uFge = g_mrp_moby_fog != 0 && gs::field(k.prim, 5, 1) != 0 ? 1 : 0;
                push.uAte = static_cast<int32_t>(gs::field(k.test, 0, 1));
                push.uAtst = static_cast<int32_t>(gs::field(k.test, 1, 3));
                push.uAref = static_cast<int32_t>(gs::field(k.test, 4, 8));
                push.uAfail = static_cast<int32_t>(gs::field(k.test, 12, 2));
                gs::DrawState ds{};
                ds.prim = k.prim;
                ds.alpha = k.alpha;
                gs::blend_state(ds);
            }
            const uint32_t kztst = gs::field(k.test, 17, 2);
            const bool ztest = gs::field(k.test, 16, 1) != 0 && kztst != 1;
            const bool zwrite = ztest && gs::field(k.zbuf, 32, 1) == 0;
            static const bool no_split = std::getenv("RAC1_GS_AFAIL_SPLIT") != nullptr && std::atoi(std::getenv("RAC1_GS_AFAIL_SPLIT")) == 0;
            const bool split = !no_split && zwrite && gs::field(k.test, 0, 1) != 0 && gs::field(k.test, 1, 3) != 1 && gs::field(k.test, 12, 2) != 0;

            MobyVkPipeKey pk{};
            pk.split_prog = split_prog;
            pk.caster = false;
            pk.depth_clamp = cur_depth_clamp;
            pk.depth_test = ztest;
            pk.depth_write = gs::field(k.zbuf, 32, 1) == 0;
            pk.depth_compare = kztst == 0 ? VK_COMPARE_OP_NEVER : kztst == 2 ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;
            pk.color_mask = 0xF;
            pk.prim = k.prim & (1ull << 6);
            pk.alpha = pk.prim ? k.alpha : 0;
            pk.color_fmt = cfmt;
            pk.depth_fmt = dfmt;
            pk.has_stencil = has_stencil;
            pk.ao_mark = has_stencil && mrp_lighting_vk_ao_marking();

            cur = k;
            have = true;
            if (split) {
                MobyVkPipeKey pk_z = pk;
                pk_z.color_mask = 0;
                push.uPass = 1;
                vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, get_moby_vk_pipeline(pk_z));
                vkCmdPushConstants(cb, g_vk_moby.pipe_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
                vkCmdDrawIndexedIndirect(cb, g_vk_moby.indirect_alloc.buffer, g_vk_moby.indirect_alloc.offset + static_cast<VkDeviceSize>(b.first) * sizeof(DrawCmd), b.count,
                                         sizeof(DrawCmd));
                ++g_st.gl_calls;
                push.uPass = 0;
                pk.depth_write = false;
                if (kztst != 0) pk.depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
            }
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, get_moby_vk_pipeline(pk));
            vkCmdPushConstants(cb, g_vk_moby.pipe_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
            vkCmdDrawIndexedIndirect(cb, g_vk_moby.indirect_alloc.buffer, g_vk_moby.indirect_alloc.offset + static_cast<VkDeviceSize>(b.first) * sizeof(DrawCmd), b.count,
                                     sizeof(DrawCmd));
            ++g_st.gl_calls;
        }
        if (!shadows_done) draw_shadows();
        own_view(false);
        return;
    }

    const SplitScope split_scope;
    Frame& F = g_frame;
    // (the frame's storage buffers bound again: uploaded at its prepare, the world's draw may have bound its own since)
    for (int i = 0; i < 4; ++i) glBindBufferBase(kGlShaderStorageBuffer, static_cast<GLuint>(i), g_ssbo[i]);
    glUseProgram(g_prog);
    glBindVertexArray(g_level.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_level.ibo);
    glBindBuffer(kGlDrawIndirectBuffer, g_indirect);
    set_common_uniforms(g_loc, win_w, win_h, vp);
    glUniform1i(g_loc.tex, 0);
    glUniform1i(g_loc.mip, g_mrp_moby_mip);
    glUniform1i(g_loc.shade, g_render_shade);
    glUniform1i(g_loc.pass, 0);
    glDisable(GL_CULL_FACE);
    if (!g_mrp_moby_keep_scissor) glDisable(GL_SCISSOR_TEST);  // (the full-screen menu's 4:3 cut kept)
    // the VU projection: the GS's depth clamped as gs_draw clamps it (the near plane: w > 0 by the side planes);
    // the world's: the GPU's near / far planes
    GLboolean depth_clamp = glIsEnabled(GL_DEPTH_CLAMP);
    if (F.mode == 1 || rt != 0) glEnable(GL_DEPTH_CLAMP);
    else glDisable(GL_DEPTH_CLAMP);
    glActiveTexture(GL_TEXTURE0);
    const int mm = g_mrp_moby_mip != 0 ? 1 : 0;
    Key cur;
    bool have = false;
    // (the drop shadows only in the world's projection and depth space; never with the modern lighting on - Daino's choice,
    // 2026-10-19 -, even where it has no sun's shadows (a zone without a sun, a view covered from it, the frames it does not
    // light): the mobys cast their own where there is a sun. The game still makes them: the frame's capture only goes undrawn)
    bool shadows_done = !shadows || F.mode != 0 || g_modern_shadows != 0;
    // the drop shadows come between the draw loop's first chain and its deferred mobys (0x24fc38 chains them there): the mobys
    // of the first chain are in the depth buffer the shadows' faces test against, the deferred ones are drawn over the darkening
    auto draw_shadows = [&] {
        shadows_done = true;
        const glm::mat4 m = world_view_proj();
        if (!mrp_shadows_draw(glm::value_ptr(m))) return;
        glUseProgram(g_prog);
        glBindVertexArray(g_level.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_level.ibo);
        glBindBuffer(kGlDrawIndirectBuffer, g_indirect);
        glActiveTexture(GL_TEXTURE0);
        have = false;  // (the state of the batches is set again)
    };
    // the boot's title main menu: its boxes' batches (Key::own_view) in the game's 4:3 projection onto the 4:3 frame (as the
    // full-screen menu's mobys: mode 1, the frame's x scale, the GS depth clamped) and cut at that frame, the scene's in the world's
    bool own_on = false;
    auto own_view = [&](bool on) {
        if (on == own_on) return;
        own_on = on;
        glUniform1i(g_loc.mode, on ? 1 : F.mode);
        float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
        if (fb_w <= 0.0f) fb_w = 512.0f;
        if (fb_h <= 0.0f) fb_h = 416.0f;
        const float xs = vp != nullptr ? vp->xscale : 1.0f;
        glUniform4f(g_loc.frame, fb_w, fb_h, on ? xs : (vp != nullptr ? vp->xscale_3d : 1.0f), 0.0f);
        if (on) mrp_lighting_bind_off(g_loc.sh);
        else if (F.mode == 0) mrp_lighting_bind_receiver(g_loc.sh);
        if (on || F.mode == 1) glEnable(GL_DEPTH_CLAMP);
        else glDisable(GL_DEPTH_CLAMP);
        if (on && vp != nullptr && xs < 1.0f) {
            const int cw = static_cast<int>(static_cast<float>(vp->w) * xs);
            glEnable(GL_SCISSOR_TEST);
            glScissor(vp->x + (vp->w - cw) / 2, vp->y, cw, vp->h);
        } else if (!g_mrp_moby_keep_scissor) {
            glDisable(GL_SCISSOR_TEST);
        }
    };
    for (const Batch& b : F.batches) {
        const Key& k = b.key;
        if (k.caster_only) continue;  // (the cached mobys outside the view: the shadow maps' only)
        if (k.rt != rt) continue;     // (a render target's mobys, drawn there)
        if (rt == 0) own_view(k.own_view);
        if (!shadows_done && k.deferred) draw_shadows();
        {  // the batch's draw commands checked against the buffers they read: one outside them is refused (logged)
            const char* why = nullptr;
            if (static_cast<uint64_t>(b.first) + b.count > F.draws.size()) why = "draw commands";
            for (uint32_t i = b.first; why == nullptr && i < b.first + b.count; ++i) {
                const DrawCmd& c = F.draws[i];
                if (static_cast<uint64_t>(c.first_index) + c.count > g_level.indices.size()) why = "indices";
                else if (c.base_vertex != 0 || c.instances != 1 || (static_cast<uint64_t>(c.base_instance) + 1) * 4 > F.cmds.size()) why = "draw records";
            }
            if (why != nullptr) {
                static uint32_t shown = 0;
                if (shown++ < 8)
                    std::fprintf(stderr, "[mrp] batch refused (%s): first %u count %u (%zu draw commands, %zu records, %zu indices)\n", why, b.first, b.count, F.draws.size(),
                                 F.cmds.size(), g_level.indices.size());
                continue;
            }
        }
        if (!have || k.bucket != cur.bucket) glBindTexture(GL_TEXTURE_2D_ARRAY, k.bucket >= 0 ? g_level.buckets[static_cast<size_t>(k.bucket)].tex : 0);
        if (!have || k.sampler != cur.sampler) glBindSampler(0, g_samplers[mm][k.sampler & 15]);
        if (!have || k.prim != cur.prim || k.test != cur.test || k.alpha != cur.alpha || k.zbuf != cur.zbuf) {
            glUniform1i(g_loc.tme, gs::field(k.prim, 4, 1) != 0 ? 1 : 0);
            glUniform1i(g_loc.fge, g_mrp_moby_fog != 0 && gs::field(k.prim, 5, 1) != 0 ? 1 : 0);
            glUniform1i(g_loc.ate, static_cast<GLint>(gs::field(k.test, 0, 1)));
            glUniform1i(g_loc.atst, static_cast<GLint>(gs::field(k.test, 1, 3)));
            glUniform1i(g_loc.aref, static_cast<GLint>(gs::field(k.test, 4, 8)));
            glUniform1i(g_loc.afail, static_cast<GLint>(gs::field(k.test, 12, 2)));
            gs::DrawState ds{};
            ds.prim = k.prim;
            ds.alpha = k.alpha;
            gs::blend_state(ds);
            const uint32_t ztst = gs::field(k.test, 17, 2);
            if (gs::field(k.test, 16, 1) != 0 && ztst != 1) {  // the GS's GEQUAL / GREATER on the host's depth: LEQUAL / LESS
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(ztst == 0 ? GL_NEVER : ztst == 2 ? GL_LEQUAL : GL_LESS);
            } else {
                glDisable(GL_DEPTH_TEST);
            }
        }
        // the GS writes the Z of the pixels that pass the alpha test (and of those failing with ZB_ONLY), the colour of those
        // that pass (and of those failing with FB_ONLY / RGB_ONLY): a translucent moby with AFAIL != KEEP (the explosions' shock-wave
        // spheres, alpha GEQUAL 8 failing RGB_ONLY) wrote Z over its whole surface and hid what is drawn after it inside (the hero)
        const uint32_t kztst = gs::field(k.test, 17, 2);
        const bool zwrite = gs::field(k.test, 16, 1) != 0 && kztst != 1 && gs::field(k.zbuf, 32, 1) == 0;
        static const bool no_split = std::getenv("RAC1_GS_AFAIL_SPLIT") != nullptr && std::atoi(std::getenv("RAC1_GS_AFAIL_SPLIT")) == 0;  // (testing: the old way)
        const bool split = !no_split && zwrite && gs::field(k.test, 0, 1) != 0 && gs::field(k.test, 1, 3) != 1 && gs::field(k.test, 12, 2) != 0;
        glDepthMask(gs::field(k.zbuf, 32, 1) == 0 ? GL_TRUE : GL_FALSE);
        cur = k;
        have = true;
        if (split) {  // (the Z first, then the colour against it with LEQUAL: an opaque part's back faces stay hidden, as per primitive on the GS)
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glUniform1i(g_loc.pass, 1);
            p_multi_draw_elements_indirect(GL_TRIANGLES, GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<uintptr_t>(b.first) * sizeof(DrawCmd)),
                                           static_cast<GLsizei>(b.count), 0);
            glUniform1i(g_loc.pass, 0);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(GL_FALSE);
            if (kztst != 0) glDepthFunc(GL_LEQUAL);
            ++g_st.gl_calls;
        }
        p_multi_draw_elements_indirect(GL_TRIANGLES, GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<uintptr_t>(b.first) * sizeof(DrawCmd)),
                                       static_cast<GLsizei>(b.count), 0);
        ++g_st.gl_calls;
        if (split) {
            glDepthMask(GL_TRUE);
            glDepthFunc(kztst == 0 ? GL_NEVER : kztst == 2 ? GL_LEQUAL : GL_LESS);
        }
    }
    if (!shadows_done) draw_shadows();
    own_view(false);
    glBindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    if (depth_clamp) glEnable(GL_DEPTH_CLAMP);
    else glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glBindVertexArray(0);
    glBindBuffer(kGlDrawIndirectBuffer, 0);
    glUseProgram(0);
}

// orig: (host) see mrp_lighting.h: the frame's mobys into the bound shadow map (their batches that write depth; alpha-tested cut-outs)
// (`sphere` (x, y, z, r): a point light's cube - the draws of the parts within its radius (+ a margin for the part's size) alone,
// their commands in a buffer of their own)
GLuint g_indirect_near = 0;
void draw_casters(const float* light_vp, const float* sphere, const float* within) {
    Frame& F = g_frame;
    if (F.mode != 0 || F.batches.empty()) return;
    if (vk::is_active() && !F.uploaded) upload_frame();
    static std::vector<std::pair<uint32_t, uint32_t>> ranges;  // (per batch: first / count in the near buffer)
    static std::vector<DrawCmd> near;
    static std::vector<uint8_t> part_vis;
    ranges.clear();
    const bool filter = sphere != nullptr || within != nullptr;
    vk::RingAlloc near_alloc{};
    if (filter) {
        const glm::vec3 c = sphere != nullptr ? glm::vec3(sphere[0], sphere[1], sphere[2]) : glm::vec3(0.0f);
        const float lim = sphere != nullptr ? sphere[3] + 6.0f : 1e30f;
        const float lim2 = sphere != nullptr ? lim * lim : 1e30f;
        // the shadows' distance: the parts within it of the camera
        const glm::vec3 wc = within != nullptr ? glm::vec3(within[0], within[1], within[2]) : glm::vec3(0.0f);
        const float wd = within != nullptr ? within[3] : 1e30f;
        const float wd2 = within != nullptr ? wd * wd : 1e30f;
        const glm::mat4 vp = (sphere != nullptr && light_vp != nullptr) ? glm::make_mat4(light_vp) : glm::mat4(1.0f);
        const size_t nparts = F.part_pos.size();
        part_vis.assign(nparts, 0);
        uint32_t any_vis = 0;
        for (size_t p = 0; p < nparts; ++p) {
            if (p < F.part_own.size() && F.part_own[p] != 0) continue;
            const glm::vec3 pos = F.part_pos[p];
            if (sphere != nullptr) {
                const glm::vec3 d = pos - c;
                if (glm::dot(d, d) > lim2) continue;
                if (light_vp != nullptr) {
                    const glm::vec4 clip = vp * glm::vec4(pos, 1.0f);
                    // Reject parts behind the cube face or well outside its 90-degree frustum (with 6.0u margin for part size)
                    if (clip.w < -6.0f || std::abs(clip.x) > clip.w + 8.5f || std::abs(clip.y) > clip.w + 8.5f) continue;
                }
            }
            if (within != nullptr) {
                const glm::vec3 dw = pos - wc;
                if (glm::dot(dw, dw) > wd2) continue;
            }
            part_vis[p] = 1;
            ++any_vis;
        }
        if (any_vis == 0) return;
        near.clear();
        ranges.reserve(F.batches.size());
        for (const Batch& b : F.batches) {
            const uint32_t first = static_cast<uint32_t>(near.size());
            const uint32_t kztst = gs::field(b.key.test, 17, 2);
            const bool zwrite = gs::field(b.key.test, 16, 1) != 0 && kztst != 1 && gs::field(b.key.zbuf, 32, 1) == 0;
            if (zwrite) {
                for (uint32_t i = b.first; i < b.first + b.count && i < F.draws.size(); ++i) {
                    const uint32_t ci = F.draws[i].base_instance;
                    const uint32_t part = 4 * ci < F.cmds.size() ? F.cmds[4 * ci] : 0xffffffffu;
                    if (part >= nparts || !part_vis[part]) continue;
                    near.push_back(F.draws[i]);
                }
            }
            ranges.push_back({first, static_cast<uint32_t>(near.size()) - first});
        }
        if (near.empty()) return;
        if (vk::is_active()) {
            near_alloc = vk::alloc_ring(near.size() * sizeof(DrawCmd), 16);
            std::memcpy(near_alloc.ptr, near.data(), near.size() * sizeof(DrawCmd));
        } else {
            if (g_indirect_near == 0) glGenBuffers(1, &g_indirect_near);
            rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_indirect_near, "MRP mobys: frame buffers");
            glBindBuffer(kGlDrawIndirectBuffer, g_indirect_near);
            glBufferData(kGlDrawIndirectBuffer, static_cast<GLsizeiptr>(near.size() * sizeof(DrawCmd)), near.data(), GL_STREAM_DRAW);
        }
    }
    if (vk::is_active()) {
        vk::ensure_rendering();
        VkCommandBuffer cb = vk::cmd();
        VkDeviceSize voff = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &g_level.vk_vbo.buffer, &voff);
        vkCmdBindIndexBuffer(cb, g_level.vk_ibo.buffer, 0, VK_INDEX_TYPE_UINT32);
        mrp_lighting_vk_bind_caster(reinterpret_cast<uint64_t>(g_vk_moby.pipe_layout), light_vp);

        MobyVkPush push{};
        fill_common_vk_push(push, nullptr);
        push.uMipMode = g_mrp_moby_mip;
        push.uShade = 0;
        push.uPass = 0;
        push.uFge = 0;

        MobyVkPipeKey pk{};
        pk.split_prog = false;
        pk.caster = true;
        pk.depth_clamp = false;
        pk.depth_test = true;
        pk.depth_write = true;
        pk.depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
        pk.color_mask = 0;
        pk.color_fmt = VK_FORMAT_UNDEFINED;
        pk.depth_fmt = VK_FORMAT_D32_SFLOAT;
        pk.has_stencil = false;
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, get_moby_vk_pipeline(pk));

        const int mm = g_mrp_moby_mip != 0 ? 1 : 0;
        const vk::RingAlloc& ind = filter ? near_alloc : g_vk_moby.indirect_alloc;
        Key cur;
        bool have = false;
        for (size_t bi = 0; bi < F.batches.size(); ++bi) {
            const Batch& b = F.batches[bi];
            const Key& k = b.key;
            const uint32_t kztst = gs::field(k.test, 17, 2);
            const bool zwrite = gs::field(k.test, 16, 1) != 0 && kztst != 1 && gs::field(k.zbuf, 32, 1) == 0;
            if (!zwrite || k.own_view) continue;
            if (static_cast<uint64_t>(b.first) + b.count > F.draws.size()) continue;
            const uint32_t dfirst = filter ? ranges[bi].first : b.first, dcount = filter ? ranges[bi].second : b.count;
            if (dcount == 0) continue;
            if (!have || k.bucket != cur.bucket || k.sampler != cur.sampler) {
                bind_moby_vk_set0(cb, k.bucket, k.sampler, mm);
            }
            if (!have || k.prim != cur.prim || k.test != cur.test) {
                push.uTme = gs::field(k.prim, 4, 1) != 0 ? 1 : 0;
                push.uAte = static_cast<int32_t>(gs::field(k.test, 0, 1));
                push.uAtst = static_cast<int32_t>(gs::field(k.test, 1, 3));
                push.uAref = static_cast<int32_t>(gs::field(k.test, 4, 8));
                push.uAfail = static_cast<int32_t>(gs::field(k.test, 12, 2));
                vkCmdPushConstants(cb, g_vk_moby.pipe_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
            }
            cur = k;
            have = true;
            vkCmdDrawIndexedIndirect(cb, ind.buffer, ind.offset + static_cast<VkDeviceSize>(dfirst) * sizeof(DrawCmd), dcount, sizeof(DrawCmd));
        }
        return;
    }
    for (int i = 0; i < 4; ++i) glBindBufferBase(kGlShaderStorageBuffer, static_cast<GLuint>(i), g_ssbo[i]);
    glUseProgram(g_prog);
    glBindVertexArray(g_level.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_level.ibo);
    glBindBuffer(kGlDrawIndirectBuffer, filter ? g_indirect_near : g_indirect);
    set_common_uniforms(g_loc, 0, 0, nullptr);
    mrp_lighting_bind_caster(g_loc.sh, light_vp);
    glUniform1i(g_loc.tex, 0);
    glUniform1i(g_loc.mip, g_mrp_moby_mip);
    glUniform1i(g_loc.shade, 0);
    glUniform1i(g_loc.pass, 0);
    glUniform1i(g_loc.fge, 0);
    glDisable(GL_CULL_FACE);
    glActiveTexture(GL_TEXTURE0);
    const int mm = g_mrp_moby_mip != 0 ? 1 : 0;
    Key cur;
    bool have = false;
    for (size_t bi = 0; bi < F.batches.size(); ++bi) {
        const Batch& b = F.batches[bi];
        const Key& k = b.key;
        const uint32_t kztst = gs::field(k.test, 17, 2);
        const bool zwrite = gs::field(k.test, 16, 1) != 0 && kztst != 1 && gs::field(k.zbuf, 32, 1) == 0;
        if (!zwrite || k.own_view) continue;
        if (static_cast<uint64_t>(b.first) + b.count > F.draws.size()) continue;
        const uint32_t dfirst = filter ? ranges[bi].first : b.first, dcount = filter ? ranges[bi].second : b.count;
        if (dcount == 0) continue;
        if (!have || k.bucket != cur.bucket) glBindTexture(GL_TEXTURE_2D_ARRAY, k.bucket >= 0 ? g_level.buckets[static_cast<size_t>(k.bucket)].tex : 0);
        if (!have || k.sampler != cur.sampler) glBindSampler(0, g_samplers[mm][k.sampler & 15]);
        if (!have || k.prim != cur.prim || k.test != cur.test) {
            glUniform1i(g_loc.tme, gs::field(k.prim, 4, 1) != 0 ? 1 : 0);
            glUniform1i(g_loc.ate, static_cast<GLint>(gs::field(k.test, 0, 1)));
            glUniform1i(g_loc.atst, static_cast<GLint>(gs::field(k.test, 1, 3)));
            glUniform1i(g_loc.aref, static_cast<GLint>(gs::field(k.test, 4, 8)));
            glUniform1i(g_loc.afail, static_cast<GLint>(gs::field(k.test, 12, 2)));
        }
        cur = k;
        have = true;
        p_multi_draw_elements_indirect(GL_TRIANGLES, GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<uintptr_t>(dfirst) * sizeof(DrawCmd)),
                                       static_cast<GLsizei>(dcount), 0);
    }
    glBindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindVertexArray(0);
    glBindBuffer(kGlDrawIndirectBuffer, 0);
    mrp_lighting_bind_off(g_loc.sh);
    glUseProgram(0);
}

// orig: (host) the shader check: every part's GPU vertices run through the vertex shader with transform
// feedback, against mrp::skin_packet (the moby's packets in order, one VU0 state) and mrp::vu1_vertex
void shader_check(const gs::Viewport* vp) {
    Frame& F = g_frame;
    const MrpFrame& f = mrp_scene();
    if (F.refs.empty()) return;
    // the TF draws need a draw record per part: appended after the frame's
    std::vector<uint32_t> cmds = F.cmds;
    const uint32_t base = static_cast<uint32_t>(cmds.size() / 4);
    for (uint32_t i = 0; i < F.refs.size(); ++i) cmds.insert(cmds.end(), {i, 1u, 0u, 0u});
    glBindBuffer(kGlShaderStorageBuffer, g_ssbo[2]);
    glBufferData(kGlShaderStorageBuffer, static_cast<GLsizeiptr>(cmds.size() * 4), cmds.data(), GL_STREAM_DRAW);
    glBindBufferBase(kGlShaderStorageBuffer, 2, g_ssbo[2]);
    size_t total = 0;
    for (const auto& r : F.refs) total += r.cls->packets[r.packet].vertex_count;
    struct TfOut {
        int32_t pos[4];
        int32_t col[4];
        float stq[4];
        int32_t xyz[4];
    };
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, g_tf_buf);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLsizeiptr>(std::max<size_t>(total, 1) * sizeof(TfOut)), nullptr, GL_STREAM_READ);
    glUseProgram(g_tf_prog);
    set_common_uniforms(g_tf_loc, 0, 0, vp);
    glBindVertexArray(g_level.vao);
    glEnable(GL_RASTERIZER_DISCARD);
    size_t at = 0;
    std::vector<size_t> offs(F.refs.size());
    for (uint32_t i = 0; i < F.refs.size(); ++i) {
        const PacketGpu& pg = F.refs[i].cls->packets[F.refs[i].packet];
        offs[i] = at;
        if (pg.vertex_count == 0) continue;
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, g_tf_buf, static_cast<GLintptr>(at * sizeof(TfOut)), static_cast<GLsizeiptr>(pg.vertex_count * sizeof(TfOut)));
        glBeginTransformFeedback(GL_POINTS);
        p_draw_arrays_instanced_base_instance(GL_POINTS, static_cast<GLint>(pg.first_vertex), static_cast<GLsizei>(pg.vertex_count), 1, base + i);
        glEndTransformFeedback();
        at += pg.vertex_count;
    }
    glDisable(GL_RASTERIZER_DISCARD);
    std::vector<TfOut> out(at);
    if (at > 0) glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, static_cast<GLsizeiptr>(at * sizeof(TfOut)), out.data());
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    // the CPU reference: per moby, its packets skinned in order on one VU0 state
    std::vector<float> normals(0x200);
    for (uint32_t i = 0; i < 0x200; ++i) normals[i] = lwc1(OV(0x00166080) + 4 * i);
    uint32_t cur_moby = 0xffffffffu;
    auto st = std::make_unique<mrp::SkinState>();
    std::vector<float> joints;
    mrp::SkinInput in;
    static int shown = 0;
    for (uint32_t i = 0; i < F.refs.size(); ++i) {
        const auto& r = F.refs[i];
        const MrpMoby& m = f.mobys[r.moby];
        const mrp::MobyClass& c = *r.cls->asset;
        if (cur_moby != r.moby || r.packet == m.first_packet) {
            cur_moby = r.moby;
            *st = mrp::SkinState{};
            joints.assign(static_cast<size_t>(m.joint_count) * 16, 0.0f);
            for (uint32_t j = 0; j < m.joint_count; ++j) std::memcpy(&joints[j * 16], &f.joints[m.joint_first + j], 64);
            in.joints = reinterpret_cast<const float(*)[16]>(joints.data());
            in.joint_count = m.joint_count;
            for (uint32_t q = 0; q < 8; ++q) std::memcpy(in.light[q].data(), &f.records[m.record_first + 0x40 + q * 0x10], 16);
            in.normal_table = reinterpret_cast<const float(*)[2]>(normals.data());
        }
        const mrp::Packet& p = c.packets[r.packet];
        std::vector<std::array<int16_t, 3>> pos;
        std::vector<std::array<uint8_t, 4>> rgba;
        mrp::skin_packet(p, in, *st, pos, rgba);
        ++g_st.chk_parts;
        {  // the CPU reference against what this frame's chain sends VU1 (the skinning's output in EE memory: as in
           // mrp_scene_asset, here on every frame of the run): positions V3-16 at 0, colours V4-8 at 97
            const MrpPart& sp = f.parts[m.part_first + (r.packet - m.first_packet)];
            const MrpUnpack* up = nullptr;
            const MrpUnpack* uc = nullptr;
            for (uint32_t u = 0; u < sp.unpack_count; ++u) {
                const MrpUnpack& un = f.unpacks[sp.unpack_first + u];
                if (un.cmd == 0x69 && un.dest == 0) up = &un;
                if (un.cmd == 0x6e && un.dest == 97) uc = &un;
            }
            if (up == nullptr || uc == nullptr) ++g_st.chk_chain_missing;
            else
                for (uint32_t s = 0; s < p.n_slots && s < up->num; ++s) {
                    ++g_st.chk_chain_slots;
                    bool pok = true, cok = true;
                    for (uint32_t l = 0; l < 3; ++l) pok = pok && std::abs(static_cast<int16_t>(lhu(up->data + s * 6 + l * 2)) - pos[s][l]) <= 1;
                    for (uint32_t l = 0; l < 4; ++l) {
                        const int32_t want = r.tint ? static_cast<int32_t>(F.parts[r.part].sub[l]) : rgba[s][l];
                        cok = cok && std::abs(static_cast<int32_t>(lbu(uc->data + s * 4 + l)) - want) <= 1;
                    }
                    if (!pok) ++g_st.chk_chain_pos;
                    if (!cok) ++g_st.chk_chain_col;
                    static int shown_c = 0;
                    if ((!pok || !cok) && shown_c++ < 10)
                        std::printf("[mrp-check] chain differs: class %d packet %u slot %u pos %d %d %d / %d %d %d col %u %u %u %u / %u %u %u %u\n", c.o_class, r.packet, s + 1,
                                    static_cast<int16_t>(lhu(up->data + s * 6)), static_cast<int16_t>(lhu(up->data + s * 6 + 2)), static_cast<int16_t>(lhu(up->data + s * 6 + 4)),
                                    pos[s][0], pos[s][1], pos[s][2], lbu(uc->data + s * 4), lbu(uc->data + s * 4 + 1), lbu(uc->data + s * 4 + 2), lbu(uc->data + s * 4 + 3),
                                    rgba[s][0], rgba[s][1], rgba[s][2], rgba[s][3]);
                }
        }
        const PartGpu& P = F.parts[r.part];
        mrp::Vu1Consts k{};
        std::memcpy(k.rows, P.rows, sizeof k.rows);
        std::memcpy(k.offset, F.offset, sizeof k.offset);
        k.proj = F.proj;
        k.fog_min = F.fog_min;
        const PacketGpu& pg = r.cls->packets[r.packet];
        for (uint32_t v = 0; v < pg.vertex_count; ++v) {
            const TfOut& o = out[offs[i] + v];
            const uint8_t slot = pg.vslots[v][0], q2 = pg.vslots[v][1];
            ++g_st.chk_vertices;
            const auto& ep = pos[slot - 1u];
            int dp = 0;
            for (int l = 0; l < 3; ++l) dp = std::max(dp, std::abs(o.pos[l] - ep[static_cast<size_t>(l)]));
            if (dp == 0) ++g_st.chk_pos_exact;
            else if (dp <= 1) ++g_st.chk_pos_1;
            else ++g_st.chk_pos_bad;
            int32_t ec[4];
            for (int l = 0; l < 4; ++l) ec[l] = r.tint ? static_cast<int32_t>(P.sub[l]) : rgba[slot - 1u][static_cast<size_t>(l)];
            if (r.tint) ++g_st.chk_tint;
            int dc = 0;
            for (int l = 0; l < 4; ++l) dc = std::max(dc, std::abs(o.col[l] - ec[l]));
            if (dc == 0) ++g_st.chk_col_exact;
            else if (dc <= 1) ++g_st.chk_col_1;
            else ++g_st.chk_col_bad;
            // the VU1 vertex from the GPU's own skinned position (the skinning compared above)
            const int32_t ip[3] = {o.pos[0], o.pos[1], o.pos[2]};
            const int32_t irgba[4] = {ec[0], ec[1], ec[2], ec[3]};
            const int32_t iuv[2] = {p.uv[slot - 1u][0], p.uv[slot - 1u][1]};
            mrp::GsVertex g = mrp::vu1_vertex(k, ip, irgba, iuv, false);
            if (q2 != 0) {  // STQ x the q2 slot's Q (kick_triangles), from the CPU's skinned q2 position
                float p2w = k.rows[3][3];
                p2w = p2w + k.rows[0][3] * static_cast<float>(pos[q2 - 1u][0]);
                p2w = p2w + k.rows[1][3] * static_cast<float>(pos[q2 - 1u][1]);
                p2w = p2w + k.rows[2][3] * static_cast<float>(pos[q2 - 1u][2]);
                float pw = k.rows[3][3];
                pw = pw + k.rows[0][3] * static_cast<float>(ip[0]);
                pw = pw + k.rows[1][3] * static_cast<float>(ip[1]);
                pw = pw + k.rows[2][3] * static_cast<float>(ip[2]);
                const float qq = (k.proj / pw) * (k.proj / p2w);
                g.s = static_cast<float>(iuv[0]) * (1.0f / 4096.0f) * qq;
                g.t = static_cast<float>(iuv[1]) * (1.0f / 4096.0f) * qq;
                g.q = 1.0f * qq;
            }
            auto rel = [](float a, float b) { return std::fabs(a - b) <= 1e-5f * std::max(1.0f, std::max(std::fabs(a), std::fabs(b))); };
            if (!rel(o.stq[0], g.s) || !rel(o.stq[1], g.t) || !rel(o.stq[2], g.q)) ++g_st.chk_stq_bad;
            const int dx = std::max(std::abs(o.xyz[0] - static_cast<int32_t>(g.x)), std::abs(o.xyz[1] - static_cast<int32_t>(g.y)));
            if (dx == 0) ++g_st.chk_xy_exact;
            else if (dx <= 1) ++g_st.chk_xy_1;
            else {
                ++g_st.chk_xy_bad;
                static int shown_xy = 0;
                if (shown_xy++ < 6)
                    std::printf("[mrp-check] class %d packet %u slot %u: X %d / %u, Y %d / %u (w %g, Q %g)\n", c.o_class, r.packet, slot, o.xyz[0], g.x, o.xyz[1], g.y, o.stq[3],
                                static_cast<double>(k.proj / o.stq[3]));
            }
            // Z: the XYZF2 24 bits (z >> 4); within 1 or 1/1024 relative (the float's rounding, 4)
            const int64_t za = (o.xyz[2] >> 4) & 0xffffff, zb = (static_cast<int32_t>(g.z) >> 4) & 0xffffff;
            if (std::llabs(za - zb) > std::max<int64_t>(1, std::max(za, zb) / 1024)) ++g_st.chk_z_bad;
            {  // the fog byte: the moby's F (micro 0x22d), or per vertex in the clipping program (0x00a), from the position the GPU skinned
                const bool clipmode = P.info[3] != 0;
                float pw = k.rows[3][3];
                pw = pw + k.rows[0][3] * static_cast<float>(ip[0]);
                pw = pw + k.rows[1][3] * static_cast<float>(ip[1]);
                pw = pw + k.rows[2][3] * static_cast<float>(ip[2]);
                const float fog = std::min(std::max((clipmode ? pw : k.rows[3][3]) + k.offset[3], k.fog_min), 255.0f);
                ++g_st.chk_fog;
                if (clipmode) ++g_st.chk_fog_clip;
                if (std::abs(o.xyz[3] - static_cast<int32_t>(fog)) > 0) ++g_st.chk_fog_bad;
            }
            if ((dp > 1 || dc > 1) && shown < 10) {
                ++shown;
                std::printf("[mrp-check] class %d packet %u slot %u: pos %d %d %d / %d %d %d col %d %d %d %d / %d %d %d %d\n", c.o_class, r.packet, slot, o.pos[0], o.pos[1], o.pos[2],
                            ep[0], ep[1], ep[2], o.col[0], o.col[1], o.col[2], o.col[3], ec[0], ec[1], ec[2], ec[3]);
            }
        }
    }
    // the frame's draw records back
    glBindBuffer(kGlShaderStorageBuffer, g_ssbo[2]);
    glBufferData(kGlShaderStorageBuffer, static_cast<GLsizeiptr>(std::max<size_t>(F.cmds.size() * 4, 16)), F.cmds.data(), GL_STREAM_DRAW);
    glBindBufferBase(kGlShaderStorageBuffer, 2, g_ssbo[2]);
    glBindBuffer(kGlShaderStorageBuffer, 0);
}

void prepare_frame(const gs::Viewport* vp) {
    if (g_frame.built) return;
    const double t0 = now_ms();
    build_frame();
    if (!vk::is_active()) {
        upload_frame();
        if (g_env_check) shader_check(vp);
    }
    ++g_st.frames;
    g_st.prof_build_ms += now_ms() - t0;
}

}  // namespace

// orig: (host) see mrp_moby_renderer.h
void mrp_moby_prepare(const gs::Viewport* vp) {
    if (g_frame.active) prepare_frame(vp);
}

// orig: (host) see mrp_moby_renderer.h
void mrp_moby_draw(int win_w, int win_h, const gs::Viewport* vp) {
    if (!g_frame.active) return;
    prepare_frame(vp);
    const double t0 = now_ms();
    if (vp != nullptr) {  // (the world's viewport: the frame's rectangle in the window)
        if (vk::is_active()) vk::set_viewport(vp->x, vp->y, vp->w, vp->h);
        else glViewport(vp->x, vp->y, vp->w, vp->h);
    }
    draw_batches(win_w, win_h, vp, true);
    if (g_env_prof) {
        if (g_env_prof_finish) {
            if (vk::is_active()) vk::wait_idle();
            else glFinish();
        }
        g_st.prof_draw_ms += now_ms() - t0;
        if (++g_st.prof_frames >= 120) {
            std::printf("[prof] mrp_mobys ms/frame: build+upload %.3f draw %.3f (%llu draws in %llu GL calls so far)\n", g_st.prof_build_ms / g_st.prof_frames,
                        g_st.prof_draw_ms / g_st.prof_frames, static_cast<unsigned long long>(g_st.draws), static_cast<unsigned long long>(g_st.gl_calls));
            g_st.prof_build_ms = g_st.prof_draw_ms = 0;
            g_st.prof_frames = 0;
        }
    }
}

// orig: (host) see mrp_moby_renderer.h
void mrp_moby_draw_target(uint32_t moby, uint64_t xyoffset, uint64_t zbuf, int w, int h) {
    if (!g_frame.active || moby == 0) return;
    prepare_frame(nullptr);
    const auto it = g_frame.rt_views.find(moby);
    if (it == g_frame.rt_views.end()) return;
    RtDraw r;
    r.view = &it->second;
    r.w = static_cast<float>(w);
    r.h = static_cast<float>(h);
    r.xo = static_cast<float>(xyoffset & 0xffff);
    r.yo = static_cast<float>((xyoffset >> 32) & 0xffff);
    // the GS's Z in the target's depth buffer (its own, cleared by the caller): window depth = 1 - Z / 2^bits of ZBUF's format (Z32 / Z24 /
    // Z16), the GEQUAL test as LEQUAL (draw_batches)
    const uint32_t psm = gs::field(zbuf, 24, 4);
    const double bits = psm == 0 ? 32.0 : psm == 1 ? 24.0 : 16.0;
    r.da = 1.0f;
    r.db = static_cast<float>(-1.0 / std::ldexp(1.0, static_cast<int>(bits)));
    g_rt_draw = &r;
    draw_batches(w, h, nullptr, false, moby);
    g_rt_draw = nullptr;
}

// orig: (host) see mrp_lighting.h
void mrp_moby_draw_casters(const float* light_vp, const float* sphere, const float* within) {
    if (!g_frame.active || !g_frame.built) return;
    draw_casters(light_vp, sphere, within);
}

// orig: (host) see mrp_lighting.h
bool mrp_moby_caster_rect(const float* light_vp, float rect[4], const float* within) {
    if (!g_frame.active || !g_frame.built || g_frame.mode != 0 || g_frame.part_pos.empty()) return false;
    const glm::mat4 vp = glm::make_mat4(light_vp);
    const float sx = glm::length(glm::vec3(vp[0][0], vp[1][0], vp[2][0])), sy = glm::length(glm::vec3(vp[0][1], vp[1][1], vp[2][1]));
    rect[0] = rect[1] = 1e30f;
    rect[2] = rect[3] = -1e30f;
    bool any = false;
    for (size_t i = 0; i < g_frame.part_pos.size(); ++i) {
        if (i < g_frame.part_own.size() && g_frame.part_own[i] != 0) continue;
        if (within != nullptr && glm::length(g_frame.part_pos[i] - glm::vec3(within[0], within[1], within[2])) > within[3]) continue;
        any = true;
        const glm::vec4 q = vp * glm::vec4(g_frame.part_pos[i], 1.0f);
        const float r = i < g_frame.part_rad.size() ? g_frame.part_rad[i] : 4.0f;
        rect[0] = std::min(rect[0], q.x - r * sx);
        rect[1] = std::min(rect[1], q.y - r * sy);
        rect[2] = std::max(rect[2], q.x + r * sx);
        rect[3] = std::max(rect[3], q.y + r * sy);
    }
    return any;
}

// orig: (host) see mrp_lighting.cpp (its declaration)
int mrp_moby_caster_parts(const float* light_vp, std::vector<float>& out, const float* within) {
    if (!g_frame.active || !g_frame.built || g_frame.mode != 0 || g_frame.part_pos.empty()) return 0;
    const glm::mat4 vp = glm::make_mat4(light_vp);
    const float sx = glm::length(glm::vec3(vp[0][0], vp[1][0], vp[2][0])), sy = glm::length(glm::vec3(vp[0][1], vp[1][1], vp[2][1]));
    int n = 0;
    for (size_t i = 0; i < g_frame.part_pos.size(); ++i) {  // (the parts mrp_moby_caster_rect takes)
        if (i < g_frame.part_own.size() && g_frame.part_own[i] != 0) continue;
        const glm::vec3 p = g_frame.part_pos[i];
        if (within != nullptr && glm::length(p - glm::vec3(within[0], within[1], within[2])) > within[3]) continue;
        const glm::vec4 q = vp * glm::vec4(p, 1.0f);
        const float r = i < g_frame.part_rad.size() ? g_frame.part_rad[i] : 4.0f;
        out.insert(out.end(), {q.x - r * sx, q.y - r * sy, q.x + r * sx, q.y + r * sy, p.x, p.y, p.z, r});
        ++n;
    }
    return n;
}

// orig: (host) see mrp_lighting.h
float mrp_moby_nearest_part(const float p[3]) {
    float best = 1e30f;
    if (!g_frame.active || !g_frame.built || g_frame.mode != 0) return best;
    const glm::vec3 c(p[0], p[1], p[2]);
    for (size_t i = 0; i < g_frame.part_pos.size(); ++i)
        if (i >= g_frame.part_own.size() || g_frame.part_own[i] == 0) best = std::min(best, glm::length(g_frame.part_pos[i] - c));
    return best;
}

// orig: (host) see mrp_lighting.h
bool mrp_moby_draw_frame(uint32_t moby, int joint, float pos[3], float axes[9], int* joints) {
    if (!g_frame.active || !g_frame.built || g_frame.mode != 0) return false;
    for (const Frame::MobyXf& x : g_frame.xf) {
        if (x.moby != moby) continue;
        if (joints != nullptr) *joints = static_cast<int>(x.joint_count);
        glm::mat4 M = x.W;  // (model -> world; a joint: model -> model, its rows the shader's m[0..3] - q = m0 x + m1 y + m2 z + m3)
        if (joint >= 0) {
            if (static_cast<uint32_t>(joint) >= x.joint_count || x.joint_first + static_cast<uint32_t>(joint) >= g_frame.joints_all.size()) return false;
            const MrpMat4& J = g_frame.joints_all[x.joint_first + static_cast<uint32_t>(joint)];
            glm::mat4 Jm(1.0f);
            for (int r = 0; r < 4; ++r) Jm[r] = glm::vec4(J.r[r].x, J.r[r].y, J.r[r].z, r == 3 ? 1.0f : 0.0f);
            M = x.W * Jm;
        }
        const glm::vec4 o = M[3];
        const float w = o.w != 0.0f ? o.w : 1.0f;
        for (int k = 0; k < 3; ++k) {
            pos[k] = o[k] / w;
            glm::vec3 a(M[k]);
            const float l = glm::length(a);
            a = l > 0.0f ? a / l : glm::vec3(k == 0, k == 1, k == 2);
            for (int c = 0; c < 3; ++c) axes[k * 3 + c] = a[c];
        }
        return std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]);
    }
    return false;
}

// orig: (host) see mrp_lighting.h
bool mrp_moby_at(const float p[3], uint32_t* moby, int* o_class, float* dist) {
    if (!g_frame.active || !g_frame.built || g_frame.mode != 0) return false;
    const glm::vec3 P(p[0], p[1], p[2]);
    float best = 1e30f;
    bool any = false;
    for (const Frame::MobyXf& x : g_frame.xf) {
        const float d = glm::length(P - x.centre);
        if (!(x.radius > 0.0f) || d > x.radius + 0.25f || x.radius >= best) continue;
        best = x.radius;
        any = true;
        *moby = x.moby;
        *o_class = x.class_id;
        *dist = glm::length(P - glm::vec3(x.W[3]) / (x.W[3].w != 0.0f ? x.W[3].w : 1.0f));
    }
    return any;
}

// orig: (host) see mrp_lighting.h
std::vector<TextureEntry> mrp_moby_texture_list(int thumb) {
    std::vector<TextureEntry> out;
    const Level& L = g_level;
    if (!L.loaded) return out;
    std::map<int, std::vector<std::pair<int, int>>> users;  // (level texture -> class, local texture)
    auto add = [&](const mrp::MobyClass& c) {
        for (const mrp::Packet& pk : c.packets)
            for (const mrp::TexturePrim& t : pk.textures)
                if (t.texture >= 0 && t.local_texture >= 0) {
                    auto& u = users[t.texture];
                    const std::pair<int, int> e{c.o_class, t.local_texture};
                    if (std::find(u.begin(), u.end(), e) == u.end()) u.push_back(e);
                }
    };
    for (const auto& [cls, a] : L.files)
        if (a != nullptr) add(*a);
    for (const auto& [ptr, cg] : L.by_ptr)
        if (cg != nullptr && cg->asset != nullptr && cg->asset->o_class >= 0) add(*cg->asset);
    auto csm1 = [](uint32_t i) { return ((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i; };
    for (size_t i = 0; i < L.level_textures && i < L.textures.size(); ++i) {
        const mrp::Texture& t = L.textures[i];
        if (t.width == 0 || t.height == 0 || t.mips.empty() || t.mips[0].size() < static_cast<size_t>(t.width) * t.height) continue;
        TextureEntry e;
        e.kind = 3;
        e.texture = static_cast<int>(i);
        e.w = static_cast<int>(t.width);
        e.h = static_cast<int>(t.height);
        if (const auto it = users.find(static_cast<int>(i)); it != users.end()) e.users = it->second;
        std::sort(e.users.begin(), e.users.end());
        e.thumb.resize(static_cast<size_t>(thumb) * thumb);
        for (int y = 0; y < thumb; ++y)
            for (int x = 0; x < thumb; ++x) {
                const uint32_t sx = static_cast<uint32_t>(x) * t.width / static_cast<uint32_t>(thumb), sy = static_cast<uint32_t>(y) * t.height / static_cast<uint32_t>(thumb);
                e.thumb[static_cast<size_t>(y) * thumb + x] = t.palette[csm1(t.mips[0][sy * t.width + sx])] | 0xff000000u;
            }
        out.push_back(std::move(e));
    }
    return out;
}

// orig: (host) see mrp_moby_renderer.h
void mrp_moby_diff_program(size_t primitives) { g_prog_marks.push_back(primitives); }

namespace {
// orig: (host) per part the MRP draws, the faithful pass's GS triangles of that program start (the k-th
// start = the scene's k-th part, as mrp_scene's check counts them) against mrp::kick_triangles on the
// chain's slot data (the asset's kick program): count, then each vertex (x / y 1/16 px, STQ, colour)
// orig: (host) the texture levels a faithful triangle may sample (mip_lo .. mip_lo + mip_n), as the GS memory held
// them at its kick, against the asset's (palette[CSM1 index]); a level the frame did not upload (0x199e80's slot 0:
// stale GS memory) is counted, not compared. `prim`: the packet's texture prim in force (-1: carried, not checked)
void check_kick_texture(const gs::Primitive& fp, const PacketGpu& pg, int prim) {
    if (fp.tex == nullptr || prim < 0 || static_cast<size_t>(prim) >= pg.prims.size()) return;
    const PrimInfo& pi = pg.prims[static_cast<size_t>(prim)];
    if (pi.bucket < 0 || pi.game_texture < 0) return;
    {  // the texture registers but the addresses (TEX0's TBP0 / TBW / CBP: the frame's VRAM slots): TEX1, CLAMP, TEX0's PSM / size / TCC / TFX / CPSM / CSM
        ++g_st.reg_tris;
        // (gs_emu keeps the primitive's TEX0 at its lowest level mip_lo: TW / TH of that level)
        constexpr uint64_t tex0_mask = (0x3full << 20) | (0x7ull << 34) | (0xfull << 51) | (0x1full << 56);
        const uint32_t lo = static_cast<uint32_t>(std::max(0, fp.mip_lo));
        const uint32_t tw = gs::field(pi.tex0, 26, 4), th = gs::field(pi.tex0, 30, 4);
        const bool same = fp.st.tex1 == pi.tex1 && fp.st.clamp == pi.clamp && (fp.st.tex0 & tex0_mask) == (pi.tex0 & tex0_mask) &&
                          gs::field(fp.st.tex0, 26, 4) == (tw > lo ? tw - lo : 0) && gs::field(fp.st.tex0, 30, 4) == (th > lo ? th - lo : 0);
        if (!same) {
            ++g_st.reg_differ;
            static int shown_r = 0;
            if (shown_r++ < 8)
                std::printf("[mrp-gs] texture %d: the faithful TEX1 %llx CLAMP %llx TEX0 %016llx / the MRP's TEX1 %llx CLAMP %llx TEX0 %016llx\n", pi.game_texture,
                            static_cast<unsigned long long>(fp.st.tex1), static_cast<unsigned long long>(fp.st.clamp), static_cast<unsigned long long>(fp.st.tex0),
                            static_cast<unsigned long long>(pi.tex1), static_cast<unsigned long long>(pi.clamp), static_cast<unsigned long long>(pi.tex0));
        }
    }
    static std::set<std::tuple<int, int, uint64_t>> seen;
    const uint32_t slot = OV(0x00199e80) + 4 * static_cast<uint32_t>(pi.game_texture);
    const mrp::Texture& tx = g_level.textures[static_cast<size_t>(g_level.buckets[static_cast<size_t>(pi.bucket)].textures[pi.layer])];
    for (int m = -1; m < fp.mip_n && m < 3; ++m) {
        const gs::TexImage* img = m < 0 ? fp.tex.get() : fp.mip_tex[m].get();
        const int l = fp.mip_lo + 1 + m;
        if (img == nullptr) continue;
        if ((l == 0 && lh(slot) == 0) || (l == 1 && lh(slot + 2) == 0)) {
            ++g_st.kick_tex_stale;
            continue;
        }
        if (!seen.insert({pi.game_texture, l, img->id}).second) continue;
        const uint32_t w = std::max(1u, tx.width >> l), h = std::max(1u, tx.height >> l);
        ++g_st.kick_tex;
        ++g_st.kick_tex_level[std::min(l, 3)];
        uint64_t bad = 0;
        if (l >= static_cast<int>(tx.mips.size()) || static_cast<uint32_t>(img->w) != w || static_cast<uint32_t>(img->h) != h || img->px.size() < w * h) bad = w * h;
        else
            for (uint32_t q = 0; q < w * h; ++q) {
                const uint32_t idx = tx.mips[static_cast<size_t>(l)][q];
                if (img->px[q] != tx.palette[((idx & 0x08) != 0) != ((idx & 0x10) != 0) ? idx ^ 0x18u : idx]) ++bad;
            }
        if (bad == 0) continue;
        ++g_st.kick_tex_bad;
        static int shown_t = 0;
        if (shown_t++ >= 8) continue;
        // the same indices through another palette? (each asset index always the same GS colour)
        uint32_t col_of[256];
        bool has[256] = {}, consistent = l < static_cast<int>(tx.mips.size()) && img->px.size() >= w * h;
        for (uint32_t q = 0; consistent && q < w * h; ++q) {
            const uint8_t idx = tx.mips[static_cast<size_t>(l)][q];
            if (!has[idx]) {
                has[idx] = true;
                col_of[idx] = img->px[q];
            } else if (col_of[idx] != img->px[q]) consistent = false;
        }
        std::printf("[mrp-tex] at the kick: texture %d level %d (TEX0 %016llx): %llu of %u texels differ%s\n", pi.game_texture, l,
                    static_cast<unsigned long long>(m < 0 ? fp.st.tex0 : fp.mip_tex0[m]), static_cast<unsigned long long>(bad), w * h,
                    consistent ? " (the same indices through another palette)" : "");
        if (consistent && g_env_debug) {  // which palette: every level / armor texture's against the colours the GS gave each index
            for (size_t o = 0; o < g_level.textures.size(); ++o) {
                const mrp::Texture& ot = g_level.textures[o];
                bool all = true;
                for (uint32_t i = 0; i < 256 && all; ++i)
                    if (has[i]) all = col_of[i] == ot.palette[((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i];
                if (all) std::printf("[mrp-tex]   the GS colours are texture %zu's palette (level textures %zu, armor base %d) armor %d\n", o, g_level.level_textures,
                                     g_level.armor_base, lh(0x0013e056));
            }
        }
    }
}

void compare_gs_triangles(const gs::Gs& gs) {
    const Frame& F = g_frame;
    const MrpFrame& f = mrp_scene();
    if (g_prog_marks.size() != 2 * f.parts.size()) {
        ++g_st.gs_frames_unmatched;
        return;
    }
    ++g_st.gs_frames;
    const auto& prims = gs.primitives();
    const float xo = static_cast<float>(F.env_xyoff & 0xffff), yo = static_cast<float>((F.env_xyoff >> 32) & 0xffff);
    // the texture prim in force along the chain (as build_frame carries it): before a packet's first switch
    const PacketGpu* carry_pg = nullptr;
    int carry_prim = -1;
    const Frame::PartRef* prev = nullptr;
    for (const auto& r : F.refs) {
        if (prev != nullptr) {
            const PacketGpu& ppg = prev->cls->packets[prev->packet];
            if (ppg.last_prim >= 0) {
                carry_pg = &ppg;
                carry_prim = ppg.last_prim;
            }
        }
        prev = &r;
        auto check_tex = [&](const gs::Primitive& fp, int prim) {
            if (prim >= 0) check_kick_texture(fp, r.cls->packets[r.packet], prim);
            else if (carry_pg != nullptr) check_kick_texture(fp, *carry_pg, carry_prim);
        };
        const MrpMoby& m = f.mobys[r.moby];
        const uint32_t pi = m.part_first + (r.packet - m.first_packet);
        const MrpPart& sp = f.parts[pi];
        const mrp::Packet& p = r.cls->asset->packets[r.packet];
        const MrpUnpack* up = nullptr;
        const MrpUnpack* uc = nullptr;
        for (uint32_t u = 0; u < sp.unpack_count; ++u) {
            const MrpUnpack& un = f.unpacks[sp.unpack_first + u];
            if (un.cmd == 0x69 && un.dest == 0) up = &un;
            if (un.cmd == 0x6e && un.dest == 97) uc = &un;
        }
        if (up == nullptr || uc == nullptr) continue;
        std::vector<std::array<int32_t, 3>> pos(p.n_slots);
        std::vector<std::array<int32_t, 4>> rgba(p.n_slots);
        for (uint32_t s = 0; s < p.n_slots && s < up->num; ++s) {
            for (uint32_t l = 0; l < 3; ++l) pos[s][l] = static_cast<int16_t>(lhu(up->data + s * 6 + l * 2));
            for (uint32_t l = 0; l < 4; ++l) rgba[s][l] = lbu(uc->data + s * 4 + l);
        }
        mrp::Vu1Consts k{};
        const PartGpu& P = F.parts[r.part];
        std::memcpy(k.rows, P.rows, sizeof k.rows);
        std::memcpy(k.offset, F.offset, sizeof k.offset);
        k.proj = F.proj;
        k.fog_min = F.fog_min;
        std::vector<mrp::GsTriangle> ref;
        mrp::kick_triangles(p, k, pos, rgba, ref, sp.mscal == 0x00a);
        ++g_st.gs_parts;
        const size_t b = g_prog_marks[2 * pi], e = g_prog_marks[2 * pi + 1];
        for (size_t i = b; i < e; ++i) {  // the GS state the faithful triangles were drawn with against the MRP's
            if (prims[i].type != 3) continue;
            const gs::DrawState& st = prims[i].st;
            ++g_st.state_tris;
            ++g_st.gs_fogcol_tris;
            if ((st.fogcol & 0xffffffull) != (gs::gs_global().reg(0x3d) & 0xffffffull)) ++g_st.gs_fogcol_bad;
            // (ALPHA's FIX matters only with C = 2: the frame environment's 0x44 and the moby pass's 0x80 << 32 | 0x44 blend alike)
            const uint64_t amask = gs::field(st.alpha, 4, 2) == 2 ? ~0ull : 0xffull;
            const bool same = (st.prim & 0x7ff) == F.prim && st.test == r.test && (gs::field(st.prim, 6, 1) == 0 || (st.alpha & amask) == (r.alpha & amask));
            if (!same) {
                ++g_st.state_differ;
                static int shown_s = 0;
                if (shown_s++ < 8)
                    std::printf("[mrp-gs] class %d packet %u: the faithful GS state prim %llx test %llx alpha %llx fba %llx / the MRP's prim %llx test %llx alpha %llx\n",
                                r.cls->asset->o_class, r.packet, static_cast<unsigned long long>(st.prim), static_cast<unsigned long long>(st.test),
                                static_cast<unsigned long long>(st.alpha), static_cast<unsigned long long>(st.fba), static_cast<unsigned long long>(F.prim),
                                static_cast<unsigned long long>(r.test), static_cast<unsigned long long>(r.alpha));
            }
        }
        if (sp.mscal == 0x00a) {  // the clipping program: its clipped / culled triangles differ by design; those it
                                  // keeps whole are matched by their screen positions and compared (STQ, colour)
            ++g_st.gs_clip_parts;
            std::map<std::array<int32_t, 6>, const mrp::GsTriangle*> by_xy;
            for (const mrp::GsTriangle& R : ref) {
                std::array<int32_t, 6> key{};
                for (int c = 0; c < 3; ++c) {
                    key[2 * c] = static_cast<int32_t>(std::lround((static_cast<float>(R.v[c].x & 0xffff) - xo) / 16.0f * 4.0f));
                    key[2 * c + 1] = static_cast<int32_t>(std::lround((static_cast<float>(R.v[c].y & 0xffff) - yo) / 16.0f * 4.0f));
                }
                by_xy[key] = &R;
            }
            for (size_t i = b; i < e; ++i) {
                if (prims[i].type != 3) continue;
                ++g_st.clip_tris;
                std::array<int32_t, 6> key{};
                for (int c = 0; c < 3; ++c) {
                    key[2 * c] = static_cast<int32_t>(std::lround(prims[i].v[static_cast<size_t>(c)].x * 4.0f));
                    key[2 * c + 1] = static_cast<int32_t>(std::lround(prims[i].v[static_cast<size_t>(c)].y * 4.0f));
                }
                auto it = by_xy.find(key);
                if (it == by_xy.end()) continue;  // a clipped triangle (or one a sub-pixel off)
                ++g_st.clip_tris_matched;
                for (int c = 0; c < 3; ++c) {
                    ++g_st.gs_fog_verts;
                    if (prims[i].v[static_cast<size_t>(c)].fog != ((it->second->v[c].f >> 4) & 0xffu)) ++g_st.gs_fog_bad;
                }
                check_tex(prims[i], it->second->texture);
                bool same = true;
                for (int c = 0; c < 3; ++c) {
                    const gs::Vertex& v = prims[i].v[static_cast<size_t>(c)];
                    const mrp::GsVertex& w = it->second->v[c];
                    same = same && v.r == w.rgba[0] && v.g == w.rgba[1] && v.b == w.rgba[2] && v.a == w.rgba[3] &&
                           std::fabs(v.q - w.q) <= 1e-4f * std::max(1.0f, std::fabs(w.q));
                }
                if (!same) {
                    ++g_st.clip_tris_differ;
                    static int shown_c = 0;
                    if (shown_c++ < 6) {
                        const gs::Vertex& v = prims[i].v[0];
                        const mrp::GsVertex& w = it->second->v[0];
                        std::printf("[mrp-gs] clipping program, class %d packet %u: kept triangle differs: rgba %u %u %u %u / %d %d %d %d, q %g / %g, prim %llx test %llx\n",
                                    r.cls->asset->o_class, r.packet, v.r, v.g, v.b, v.a, w.rgba[0], w.rgba[1], w.rgba[2], w.rgba[3], static_cast<double>(v.q),
                                    static_cast<double>(w.q), static_cast<unsigned long long>(prims[i].st.prim), static_cast<unsigned long long>(prims[i].st.test));
                    }
                }
            }
            continue;
        }
        size_t n = 0;
        for (size_t i = b; i < e; ++i) n += prims[i].type == 3 ? 1 : 0;
        g_st.gs_tris_ref += ref.size();
        g_st.gs_tris_faithful += n;
        if (n != ref.size()) {
            ++g_st.gs_parts_count_differ;
            continue;
        }
        size_t t = 0;
        bool part_ok = true;
        for (size_t i = b; i < e; ++i) {
            if (prims[i].type != 3) continue;
            const mrp::GsTriangle& R = ref[t++];
            for (int c = 0; c < 3; ++c) {
                ++g_st.gs_fog_verts;
                if (prims[i].v[static_cast<size_t>(c)].fog != ((R.v[c].f >> 4) & 0xffu)) ++g_st.gs_fog_bad;
            }
            bool ok = true;
            for (int c = 0; c < 3; ++c) {
                const gs::Vertex& v = prims[i].v[static_cast<size_t>(c)];
                const mrp::GsVertex& w = R.v[c];
                const float wx = (static_cast<float>(w.x & 0xffff) - xo) / 16.0f, wy = (static_cast<float>(w.y & 0xffff) - yo) / 16.0f;
                auto rel = [](float a, float bb) { return std::fabs(a - bb) <= 1e-4f * std::max(1.0f, std::max(std::fabs(a), std::fabs(bb))); };
                ok = ok && std::fabs(v.x - wx) <= 0.07f && std::fabs(v.y - wy) <= 0.07f && rel(v.s, w.s) && rel(v.t, w.t) && rel(v.q, w.q) && v.r == w.rgba[0] && v.g == w.rgba[1] &&
                     v.b == w.rgba[2] && v.a == w.rgba[3];
            }
            if (!ok) {
                ++g_st.gs_tris_differ;
                part_ok = false;
            }
            check_tex(prims[i], R.texture);
        }
        if (!part_ok) {
            ++g_st.gs_parts_differ;
            static int shown = 0;
            if (shown++ < 10) std::printf("[mrp-gs] class %d packet %u: the faithful GS triangles differ from the kick program's\n", r.cls->asset->o_class, r.packet);
        }
    }
}
}  // namespace

// orig: (host) see mrp_moby_renderer.h
void mrp_moby_diff(int win_w, int win_h, const gs::Viewport* vp, const gs::Gs& gs, float fb_w, float fb_h, uint64_t frame, const gs::DepthMap* depth) {
    if (!g_frame.active) return;
    if (g_env_check) {
        prepare_frame(vp);
        compare_gs_triangles(gs);
    }
    const int vw = vp != nullptr ? vp->w : win_w, vh = vp != nullptr ? vp->h : win_h;
    if (vw <= 0 || vh <= 0) return;
    static GLuint fbo[2] = {}, tex[2] = {}, rbo[2] = {};
    static int fw = 0, fh = 0;
    if (fbo[0] == 0 || fw != vw || fh != vh) {
        for (int i = 0; i < 2; ++i) {
            if (fbo[i] == 0) {
                glGenFramebuffers(1, &fbo[i]);
                glGenTextures(1, &tex[i]);
                rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex[i], "MRP mobys: check targets (testing)");
                glGenRenderbuffers(1, &rbo[i]);
                rac1::prof::gl_track_n(rac1::prof::kGlRenderbuffer, 1, &rbo[i], "MRP mobys: check targets (testing)");
            }
            glBindTexture(GL_TEXTURE_2D, tex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vw, vh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo[i]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, vw, vh);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo[i]);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        fw = vw;
        fh = vh;
    }
    GLint prev_fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
    gs::Viewport lv = vp != nullptr ? *vp : gs::Viewport{};
    lv.x = 0;
    lv.y = 0;
    lv.w = vw;
    lv.h = vh;
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glViewport(0, 0, vw, vh);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 0);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (i == 0) gs::draw(gs, vw, vh, fb_w, fb_h, depth, &lv, 1);  // the faithful pass's moby primitives
        else {
            prepare_frame(&lv);
            draw_batches(vw, vh, &lv);
        }
    }
    std::vector<uint8_t> a(static_cast<size_t>(vw) * vh * 4), b(a.size());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
    glReadPixels(0, 0, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, a.data());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
    glReadPixels(0, 0, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, b.data());
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev_fbo));
    uint64_t na = 0, nb = 0, both = 0, oa = 0, ob = 0, d8 = 0, sum = 0;
    for (size_t k = 0; k < a.size(); k += 4) {
        const bool da = a[k + 3] > 0, db = b[k + 3] > 0;
        na += da;
        nb += db;
        if (da && db) {
            ++both;
            int m = 0;
            for (int c = 0; c < 3; ++c) {
                const int d = std::abs(static_cast<int>(a[k + c]) - static_cast<int>(b[k + c]));
                m = std::max(m, d);
                sum += static_cast<uint64_t>(d);
            }
            if (m > 8) ++d8;
        } else if (da) ++oa;
        else if (db) ++ob;
    }
    ++g_st.diff_frames;
    g_st.px_a += na;
    g_st.px_b += nb;
    g_st.px_both += both;
    g_st.px_only_a += oa;
    g_st.px_only_b += ob;
    g_st.px_diff8 += d8;
    g_st.px_sum_abs += sum;
    if (g_env_debug && std::getenv("RAC1_MRP_DIFF_SHOTS") != nullptr && std::strstr(std::getenv("RAC1_MRP_DIFF_SHOTS"), std::to_string(frame).c_str()) != nullptr) {
        const MrpFrame& f = mrp_scene();
        for (const MrpMoby& m : f.mobys) {  // (testing: where each moby's origin is on the frame)
            const MrpMat4& r = m.part_count > 0 ? f.parts[m.part_first].model_to_view : m.model_to_view;
            const float q = f.proj[1].x / r.r[3].w;
            std::printf("[mrp-moby] frame %llu moby %08x class %d flags %04x parts %u packets %u deferred %d shadow %d mscal %x origin px %.1f %.1f\n",
                        static_cast<unsigned long long>(frame), m.moby, m.class_id, m.flags, m.part_count, m.packet_count, m.deferred ? 1 : 0, m.shadow_mesh ? 1 : 0,
                        m.part_count > 0 ? f.parts[m.part_first].mscal : 0, r.r[3].x * q + f.proj[5].x - 2048.0f + fb_w * 0.5f, r.r[3].y * q + f.proj[5].y - 2048.0f + fb_h * 0.5f);
        }
    }
    uint32_t clip_parts = 0;  // parts the VU's clipping program draws (MSCAL 0xa: the moby crosses the guard band)
    for (const MrpPart& p : mrp_scene().parts) clip_parts += p.mscal == 0x00a ? 1u : 0u;
    std::printf("[mrp-diff] frame %llu: faithful %llu px, mrp %llu px, both %llu (colour diff > 8: %llu, mean abs %.2f), only faithful %llu, only mrp %llu; clipping-program parts %u\n",
                static_cast<unsigned long long>(frame), static_cast<unsigned long long>(na), static_cast<unsigned long long>(nb), static_cast<unsigned long long>(both),
                static_cast<unsigned long long>(d8), both > 0 ? static_cast<double>(sum) / (3.0 * static_cast<double>(both)) : 0.0, static_cast<unsigned long long>(oa),
                static_cast<unsigned long long>(ob), clip_parts);
    if (const char* shots = std::getenv("RAC1_MRP_DIFF_SHOTS")) {  // the two images (and their difference) at the frames listed
        for (const char* q = shots; *q != 0;) {
            char* e;
            const unsigned long fr = std::strtoul(q, &e, 10);
            if (e == q) break;
            if (fr == frame) {
                for (int i = 0; i < 3; ++i) {
                    char fn[64];
                    std::snprintf(fn, sizeof fn, "mrp_diff_%lu_%s.ppm", fr, i == 0 ? "faithful" : i == 1 ? "mrp" : "delta");
                    if (FILE* fp = std::fopen(fn, "wb")) {
                        std::fprintf(fp, "P6\n%d %d\n255\n", vw, vh);
                        for (int y = vh - 1; y >= 0; --y)
                            for (int x = 0; x < vw; ++x) {
                                const size_t o = (static_cast<size_t>(y) * vw + x) * 4;
                                uint8_t px[3];
                                for (int c = 0; c < 3; ++c)
                                    px[c] = i == 0 ? a[o + c] : i == 1 ? b[o + c]
                                                                   : static_cast<uint8_t>(std::min(255, 4 * std::abs(static_cast<int>(a[o + c]) - static_cast<int>(b[o + c]))));
                                if (i == 2 && (a[o + 3] > 0) != (b[o + 3] > 0)) {  // coverage: red only faithful, green only MRP
                                    px[0] = a[o + 3] > 0 ? 255 : 0;
                                    px[1] = b[o + 3] > 0 ? 255 : 0;
                                    px[2] = 0;
                                }
                                std::fwrite(px, 1, 3, fp);
                            }
                        std::fclose(fp);
                    }
                }
            }
            q = *e == ',' ? e + 1 : e;
        }
    }
}

// orig: (host) see mrp_moby_renderer.h
void mrp_moby_report() {
    if (g_st.frames == 0) return;
    const Stats& s = g_st;
    auto u = [](uint64_t v) { return static_cast<unsigned long long>(v); };
    std::printf("[mrp] load: %llu classes, %llu packets, %llu GPU vertices (%llu with STQ x a second Q), %llu triangles; kick ops mixing slots %llu, program-start Q %llu; "
                "matrices unresolved %llu, misaligned %llu, blends of blends %llu; runs on the previous texture %llu\n",
                u(s.classes), u(s.packets), u(s.vertices), u(s.q2_vertices), u(s.triangles), u(s.mixed_ops), u(s.startq_ops), u(s.unresolved_matrix), u(s.misaligned), u(s.nested_blend),
                u(s.prev_texture_runs));
    std::printf("[mrp] textures: prims from the asset %llu, from the class in memory %llu, white %llu, missing %llu, asset / memory texture mismatches %llu, MXL above the mips %llu\n",
                u(s.prims_asset), u(s.prims_memory), u(s.prims_white), u(s.prims_missing), u(s.prims_mismatch), u(s.mxl_over_mips));
    std::printf("[mrp] frames %llu: mobys %llu (without an asset %llu), parts %llu (shadow-mesh parts not drawn %llu, fewer parts than packets %llu), draws %llu in %llu GL calls\n",
                u(s.frames), u(s.mobys), u(s.no_asset_mobys), u(s.parts), u(s.shadow_parts), u(s.part_mismatch), u(s.draws), u(s.gl_calls));
    if (g_env_debug) std::printf("[mrp] world matrix: the rows' origin against the moby's position, max %.6f m\n", s.world_err_max);
    if (s.cc_mobys != 0)
        std::printf("[mrp-class] %llu moby draws checked: %llu whose asset is not the class in EE memory (class id or first packet's vertex data)\n", u(s.cc_mobys), u(s.cc_bad));
    if (g_env_check)
        std::printf("[mrp-check] triangles: %llu packets, %llu differ from kick_triangles; vertex shader: %llu parts, %llu vertices: position exact %llu, within 1 %llu, worse %llu; "
                    "colour exact %llu, within 1 %llu, worse %llu (sub-mesh colour %llu); STQ off %llu; XY exact %llu, within 1/16 px %llu, worse %llu; Z off %llu\n",
                    u(s.tri_check_packets), u(s.tri_check_bad), u(s.chk_parts), u(s.chk_vertices), u(s.chk_pos_exact), u(s.chk_pos_1), u(s.chk_pos_bad), u(s.chk_col_exact),
                    u(s.chk_col_1), u(s.chk_col_bad), u(s.chk_tint), u(s.chk_stq_bad), u(s.chk_xy_exact), u(s.chk_xy_1), u(s.chk_xy_bad), u(s.chk_z_bad));
    if (g_env_check)
        std::printf("[mrp-check] fog byte: %llu vertices (%llu of the clipping program's per-vertex F), %llu differ\n", u(s.chk_fog), u(s.chk_fog_clip), u(s.chk_fog_bad));
    if (g_env_check)
        std::printf("[mrp-check] the reference against the frame's chain (what VU1 gets): %llu slots, positions off by more than 1: %llu, colours off by more than 1: %llu; "
                    "parts without the two UNPACKs %llu\n",
                    u(s.chk_chain_slots), u(s.chk_chain_pos), u(s.chk_chain_col), u(s.chk_chain_missing));
    if (s.gs_frames + s.gs_frames_unmatched > 0)
        std::printf("[mrp-gs] the faithful pass's GS triangles against the kick programs: %llu frames (%llu not matched to the scene), %llu parts (%llu by the clipping program, not "
                    "compared); triangles %llu faithful / %llu kick program; parts with another count %llu, parts with differing triangles %llu (%llu triangles)\n",
                    u(s.gs_frames), u(s.gs_frames_unmatched), u(s.gs_parts), u(s.gs_clip_parts), u(s.gs_tris_faithful), u(s.gs_tris_ref), u(s.gs_parts_count_differ),
                    u(s.gs_parts_differ), u(s.gs_tris_differ));
    if (s.state_tris > 0)
        std::printf("[mrp-gs] the GS state (PRIM, TEST, ALPHA when blending) of %llu faithful triangles: %llu with another than the MRP's; texture registers of %llu: %llu differ\n",
                    u(s.state_tris), u(s.state_differ), u(s.reg_tris), u(s.reg_differ));
    if (s.gs_fogcol_tris > 0)
        std::printf("[mrp-gs] FOGCOL of the faithful GS triangles against the register the MRP reads at its draw: %llu triangles, %llu differ\n", u(s.gs_fogcol_tris), u(s.gs_fogcol_bad));
    if (s.gs_fog_verts > 0)
        std::printf("[mrp-gs] the faithful GS triangles' fog bytes (XYZF2 F) against the kick program's: %llu vertices, %llu differ\n", u(s.gs_fog_verts), u(s.gs_fog_bad));
    if (s.clip_tris > 0)
        std::printf("[mrp-gs] the clipping program's parts: %llu faithful triangles, %llu found among the kick program's by their screen position, %llu of those with "
                    "another colour / Q\n",
                    u(s.clip_tris), u(s.clip_tris_matched), u(s.clip_tris_differ));
    if (s.kick_tex > 0)
        std::printf("[mrp-tex] the asset textures against the images the faithful triangles sampled (GS memory at the kick, each texture / level / image once): %llu "
                    "(levels 0 / 1 / 2 / 3: %llu / %llu / %llu / %llu), differing %llu; levels the frame did not upload (stale GS memory, not compared) %llu\n",
                    u(s.kick_tex), u(s.kick_tex_level[0]), u(s.kick_tex_level[1]), u(s.kick_tex_level[2]), u(s.kick_tex_level[3]), u(s.kick_tex_bad), u(s.kick_tex_stale));
    if (s.diff_frames > 0)
        std::printf("[mrp-diff] %llu frames: faithful %llu px, mrp %llu px, both %llu (colour diff > 8: %llu = %.3f%%, mean abs %.3f), only faithful %llu (%.3f%%), only mrp %llu (%.3f%%)\n",
                    u(s.diff_frames), u(s.px_a), u(s.px_b), u(s.px_both), u(s.px_diff8), s.px_both ? 100.0 * static_cast<double>(s.px_diff8) / static_cast<double>(s.px_both) : 0.0,
                    s.px_both ? static_cast<double>(s.px_sum_abs) / (3.0 * static_cast<double>(s.px_both)) : 0.0, u(s.px_only_a),
                    s.px_a ? 100.0 * static_cast<double>(s.px_only_a) / static_cast<double>(s.px_a) : 0.0, u(s.px_only_b),
                    s.px_b ? 100.0 * static_cast<double>(s.px_only_b) / static_cast<double>(s.px_b) : 0.0);
    std::fflush(stdout);
}

// orig: (host) see mrp_lighting.h
uint32_t mrp_moby_class_local_textures(int o_class) {
    uint32_t bits = 0;
    auto add = [&](const mrp::MobyClass& c) {
        for (const mrp::Packet& p : c.packets)
            for (const mrp::TexturePrim& t : p.textures)
                if (t.local_texture >= 0 && t.local_texture < 32) bits |= 1u << t.local_texture;
    };
    for (const auto& [ptr, cg] : g_level.by_ptr)
        if (cg != nullptr && cg->asset != nullptr && cg->asset->o_class == o_class) add(*cg->asset);
    if (const auto it = g_level.files.find(o_class); bits == 0 && it != g_level.files.end() && it->second != nullptr) add(*it->second);
    return bits;
}

}  // namespace rac1
