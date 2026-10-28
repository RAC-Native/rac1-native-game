// orig: (host) The MRP world renderer (see mrp_world_renderer.h).
#include <game/render/mrp_world_renderer.h>
#include <game/render/mrp_chain.h>

#include <formats/moby_mrp.h>
#include <formats/world_mrp.h>
#include <game/boot/front_end.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/world_vu1.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_lod.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_tfrag_gpu.h>
#include <game/render/mrp_world_assets.h>
#include <game/render/mrp_world_check.h>
#include <game/render/mrp_world_scene.h>
#include <game/render/mrp_world_static.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <profiler/host_profiler.h>
#include <runtime/gs/gs_draw.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace rac1 {

int g_mrp_world_projection = 0;
int g_mrp_world_mip = 0;
int g_mrp_world_fog = 1;

namespace {

using namespace mrpw;

const bool g_env_check = std::getenv("RAC1_WORLD_MRP_CHECK") != nullptr;
const bool g_env_diff = std::getenv("RAC1_WORLD_MRP_DIFF") != nullptr;
const bool g_env_prof = std::getenv("RAC1_FRAME_PROF") != nullptr;
// (RAC1_FRAME_PROF=2, no glFinish inside the stages: the host times them with GPU queries)
const bool g_env_prof_finish = g_env_prof && std::atoi(std::getenv("RAC1_FRAME_PROF")) <= 1;
const bool g_env_debug = std::getenv("RAC1_WORLD_MRP_DEBUG") != nullptr;

double now_ms() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

std::string asset_root() {
    const char* r = std::getenv("RAC1_MRPW_DIR");
    return r != nullptr ? r : "assets/generated/mrp/world";
}

// the assets' level: the level 0x15ed84 names, or the boot module's title level while the boot runs (the title loop
// sets 0x15ed84 to 0, its scene is its own level)
// (while the loading screen shows, its own scene: the flight's assets)
int world_level_id() { return level_exit_loading_screen() ? kFlightLevel : boot_active() ? kTitleLevel : lw(0x0015ed84); }

// ---- level state -------------------------------------------------------------------------------

struct Bucket {
    uint32_t w = 0, h = 0, levels = 0;
    GLuint tex = 0;
    vk::GpuImage vk_tex{};
    std::vector<std::pair<int, int>> textures;  // (table, index) per layer
};
struct TexRef {
    int bucket = -1;
    uint32_t layer = 0, mips = 1;
};
// The GPU path's static data of a tie / shrub packet (its kick program evaluated once, by the GS's rules): one GPU vertex per
// kicked vertex (its sources), the triangles as runs of one texture slot and PRIM.
struct TVert {
    int16_t pos[4];    // the position (tie fat: the base), flags (1: a tie fat vertex)
    int16_t delta[4];  // tie fat: the delta the morph factor scales
    int16_t uv[4];     // s, t, q (shrub h), x 4096
    uint8_t col[4];    // the colour indices (tie fat: c0, c1, c2)
};
static_assert(sizeof(TVert) == 28);
struct TplRun {
    uint32_t first = 0, count = 0;  // indices, relative to the template's
    int slot = -1;                  // the texture slot / prim in force (-1: the GS's before the packet)
    uint64_t prim = 0;
};
struct Tpl {
    uint32_t first_vertex = 0, first_index = 0;
    std::vector<TplRun> runs;
    int last_slot = -1;  // the packet's last texture switch
    uint64_t last_prim = 0;
    bool has_prim = false;
};

struct Level {
    int id = -1;
    bool loaded = false, tried = false;
    std::unique_ptr<World> world;
    std::unique_ptr<WorldIndex> idx;
    std::vector<mrp::Texture> tex[3];  // tfrag, tie, shrub
    std::vector<TexRef> ref[3];
    std::vector<Bucket> buckets;
    GLuint vao = 0, vbo = 0, ibo = 0;
    bool reset_walker = true;
    // the GPU path
    std::unordered_map<const void*, Tpl> tpl;  // TiePacket / ShrubPacket -> its template
    std::vector<TVert> tverts;
    std::vector<uint32_t> tidx;
    GLuint tvao = 0, tvbo = 0, tibo = 0;
    vk::GpuBuffer vk_tvbo{}, vk_tibo{};
    // the sky's textures: decoded from the sky asset's data in EE memory at their first use (the image / palette the
    // texture uploads of the sky's chain send), each a bucket of its own; by (image, palette, TEX0's format and size fields)
    std::map<std::tuple<uint32_t, uint32_t, uint64_t>, TexRef> sky_tex;
    // the world GIF streams' textures (the shrub billboards: the level's GS RAM block gs_ram_upload 0x240498 put in GS
    // memory at the level load), decoded from the GS replica's local memory at their first use; by (TEX0 but TCC / TFX, MIPTBP1, mips)
    std::map<std::tuple<uint64_t, uint64_t, uint32_t>, TexRef> vram_tex;
};
Level g_level;
// (stage 2) the level's classes as the passes find them, the frame's host-made ties / shrubs, the check switch
StaticLevel g_slevel;
// the level's tfrags for the GPU path (mrp_tfrag_gpu.h), their resident lit colours, the VAO over the vertices / indices the
// compute program writes; RAC1_MRPW_TFRAG_VERIFY=1: the GPU's vertices / triangles read back and compared with the CPU reference's
TfragLevel g_tlevel;
TfragColours g_tcols;
GLuint g_tfrag_vao = 0;
// The GPU path's draws find their command (cmds[2 x i]) by an instanced vertex attribute (location 8, divisor 1) over the
// integers 0, 1, 2..: a draw's base instance offsets it, as the core profile does for every instanced attribute (the hardware's own
// instance start). Not gl_BaseInstance, a value the driver supplies itself: on an Intel UHD 620 under Windows the ties, shrubs and mobys
// were drawn now and then with other draws' records (the world in pieces, issue #113), as a wrong gl_BaseInstance draws them. The
// same values, the same picture elsewhere. The buffer is shared by the levels' VAOs, grown as needed. (Vulkan: gl_InstanceIndex.)
GLuint g_draw_ids = 0;
size_t g_draw_ids_n = 0;
// orig: (host) the draw indices' buffer holding at least `n` (bound to GL_ARRAY_BUFFER after)
void draw_ids_reserve(size_t n) {
    if (g_draw_ids == 0) {
        glGenBuffers(1, &g_draw_ids);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_draw_ids, "MRP world: draw indices");
    }
    glBindBuffer(GL_ARRAY_BUFFER, g_draw_ids);
    if (n <= g_draw_ids_n) return;
    size_t m = std::max<size_t>(g_draw_ids_n, 4096);
    while (m < n) m *= 2;
    std::vector<uint32_t> ids(m);
    for (size_t i = 0; i < m; ++i) ids[i] = static_cast<uint32_t>(i);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m * sizeof(uint32_t)), ids.data(), GL_STATIC_DRAW);
    g_draw_ids_n = m;
}
const bool g_tfrag_verify = std::getenv("RAC1_MRPW_TFRAG_VERIFY") != nullptr;
// (RAC1_MRPW_TFRAG_CHECK=1: the host's tfrag decisions made at the game's own distances / frustum and compared with the walker's draws,
// nothing replaced) the frame's decisions (made in the capture step from the snapshot the tfrag pass left)
const bool g_tfrag_check = std::getenv("RAC1_MRPW_TFRAG_CHECK") != nullptr;
std::vector<std::pair<uint32_t, uint8_t>> g_tdecide;
std::vector<std::pair<uint32_t, uint8_t>> g_tshadow;  // the tfrag casters outside the view
std::vector<uint16_t> g_tshadow_views;  // their shadow views
bool g_tfrag_shadow_on = false;  // this frame's scene has the tfrag casters (SceneInput::tfrag_shadow)
StaticFrame g_sframe;
// the modern lighting's ties / shrubs outside the view whose shadows can fall into it (mrp_world_static.h ShadowCull)
StaticFrame g_shframe;
const bool g_static_check = std::getenv("RAC1_MRPW_STATIC_CHECK") != nullptr;

// One GPU vertex (an XYZF2 the programs kick): its world point (homogeneous), its point on the GS's frame (x w), the STQ
// undivided ((S, T, Q) x w: S / Q is the texture coordinate), the GS's Q (the mip level), RGBA, F.
struct GpuV {
    float wp[4];
    float vu[4];
    float stq[3];
    float lodq;
    uint8_t rgba[4];
    float fog;
    uint32_t word, layer;  // the texture (the triangle this vertex closes: flat, the last vertex provokes)
    uint32_t state[2];     // its GS state as data (draw_flags, FOGCOL), flat as the texture
};
static_assert(sizeof(GpuV) == 72);

// the GL state of a run of triangles
struct Key {
    int bucket = -1;
    uint32_t layer = 0, word = 1, sampler = 0;
    uint64_t prim = 0, test = 0, alpha = 0, zbuf = 0;
    uint32_t fogcol = 0;
    bool operator==(const Key&) const = default;
};
// what a batch's draws share: the GL state the GS state maps to (blending: PRIM ABE + ALPHA; the depth test; the depth
// write; the alpha test's second pass, AFAIL), and for the CPU path its texture (bucket, sampler); the rest of the GS state (TME, IIP,
// FGE, the alpha test itself, FOGCOL; the GPU path's texture unit) is per draw data (draw_flags), so runs that differ only in it are
// one multi-draw
struct RKey {
    bool gpu = false, sky = false;
    bool shadow = false;  // the modern lighting's casters outside the view: drawn into the shadow maps only
    uint16_t views = 0;   // a caster's shadow views (bits 0..2 the cascades, 3 + i point light slot i)
    bool view_only = false;  // the frame's shrubs while the lighting casts its own (full models): not drawn into the shadow maps
    uint32_t abe = 0, ztest = 0, zwrite = 0, pass2 = 0, sampler = 0, fogcol = 0;
    uint64_t alpha = 0;
    int bucket = -1;
    bool operator==(const RKey&) const = default;
};
struct Batch {
    Key key;
    bool gpu = false;               // the GPU path's draws (word / layer in its draw records)
    bool sky = false;               // the sky's (in the world projection, directions: the depth clamped to the far plane)
    uint32_t first = 0, count = 0;  // indices (CPU path) / draw commands (GPU path)
    RKey rk;
    int epoch = -1;                 // the GPU path's texture units (Frame::epochs)
    bool tfrag = false;             // the GPU path's tfrag triangles (first / count: in the compute program's indices)
    bool ate = false;               // one of its draws has the alpha test on (the shadow pass's depth-only program not for it)
};
struct InstGpu {
    float clip[16];  // model -> GL clip (the world's projection)
    float rows[16];  // R0, R1, R2, T
    float morph[4];
    uint32_t info[4];  // colour first, F, alpha, kind (0 tie, 1 shrub)
};
struct DrawCmd {  // DrawElementsIndirectCommand
    uint32_t count, instances, first_index;
    int32_t base_vertex;
    uint32_t base_instance;
};

struct Stats {
    uint64_t gpu_packets = 0;
    uint64_t tfrag_gpu_draws = 0, tfrag_cpu_draws = 0, tfrag_checked = 0, tfrag_bad = 0, tfrag_host = 0;
    uint64_t tv_frames = 0, tv_verts = 0, tv_vbad = 0, tv_tris = 0, tv_tbad = 0, tv_field[8] = {};
    uint64_t tchk_walker = 0, tchk_found = 0, tchk_missing = 0, tchk_extra = 0, tchk_code = 0;
    uint64_t col_resident = 0, col_copied = 0, col_verified = 0, col_bad = 0;
    uint64_t stat_draws[2] = {}, chk_walker[2] = {}, chk_found[2] = {}, chk_missing[2] = {}, chk_extra[2] = {};
    uint64_t stat_variants = 0, chk_gif_frames = 0, chk_gif_bad = 0, chk_gif_quads = 0;
    uint64_t chk_morph_bad = 0, chk_fog_bad = 0, chk_alpha_bad = 0, chk_rows_bad = 0;
    double chk_rows_maxrel = 0;
    std::vector<std::string> chk_notes;
    uint64_t frames = 0, scene_frames = 0, packets = 0, vertices = 0, triangles = 0, adc = 0, clip_dropped = 0, batches = 0;
    uint64_t draws[6] = {}, unknown[6] = {}, setups = 0, unknown_program = 0;
    uint64_t tex_white = 0, tex_missing = 0, tex_ok = 0;
    uint64_t gif_packets = 0, gif_vertices = 0, gif_triangles = 0, gif_textures = 0, gif_tex_missing = 0;
    uint64_t sky_frames = 0, sky_packets = 0, sky_kicks = 0, sky_vertices = 0, sky_triangles = 0, sky_sprites = 0, sky_other = 0, sky_tex_missing = 0, sky_textures = 0;
    double ms_sky = 0, ms_sky_scene = 0;
    double ms_scene = 0, ms_build = 0, ms_draw = 0, ms_submit = 0, ms_wait = 0;
    int prof_frames = 0;
    // image diff
    uint64_t diff_frames = 0, px_a = 0, px_b = 0, px_both = 0, px_only_a = 0, px_only_b = 0, px_diff8 = 0, px_sum_abs = 0;
};
Stats g_st;
CheckStats g_check;

struct Frame {
    bool active = false;
    SceneFrame scene;
    std::vector<GpuV> v;
    std::vector<uint32_t> idx;
    std::vector<Batch> batches;
    std::vector<InstGpu> insts;
    std::vector<uint32_t> cmds;  // 8 words per draw command: instance, texture word, layer, draw_flags; FOGCOL, 0, 0, 0
    // the GPU path's texture units: per epoch the (bucket, sampler) bound to units 1; a new epoch when they are full
    std::vector<std::vector<std::pair<int, uint32_t>>> epochs;
    std::vector<uint32_t> inst_of;  // the scene's instance -> its record in `insts`
    // the alpha test's second pass (AFAIL) made once for consecutive draws of different GS states that share the GL
    // state (RKey), as it already was for draws of one GS state: the GS writes a primitive's failing pixels before the next primitive,
    // the two passes write all the batch's passing pixels first - differs where draws of the batch overlap. Off (the earlier batching,
    // one batch per GS state) at the original settings, so that their image is unchanged; on when the host makes the ties / shrubs
    // (LOD > 0, widescreen), where the batches would otherwise be ~2500 a frame. RAC1_MRPW_MERGE_AFAIL=0 / 1: never / always.
    bool merge_afail = false;
    bool uploaded = false;  // (the frame's GPU data sent: the modern lighting draws it more than once)
    bool shadow_casters = false;  // the frame has the casters' batches (RKey::shadow): the shadow pass draws them for the ties / shrubs
    std::vector<DrawCmd> draws;
    std::vector<std::pair<uint32_t, uint32_t>> skips;
    uint64_t env_xyoff = 0;
    // The scene built on the worker thread while the host runs the GS replica's chain (mrp_world_begin_frame_async /
    // mrp_world_finish_frame): the skip ranges known before (they depend on the passes alone), the GS registers as the replica had
    // them before the chain (build_triangles' start)
    bool skips_ready = false;
    bool pending = false;
    std::array<uint64_t, 128> gs_regs{};
    // the GPU path's tfrag draws (mrp_tfrag_gpu.h); with RAC1_MRPW_TFRAG_VERIFY the CPU reference's vertices / triangles of
    // each (GsFront on its kicks: (first vertex, vertices, the triangles' local indices))
    TfragGpuFrame tf;
    struct TfragExpect {
        uint32_t vbase = 0, nverts = 0;
        std::vector<GpuV> v;
        std::vector<uint32_t> tri;
    };
    std::vector<TfragExpect> tf_expect;
    // what build_triangles reads of the host's state, taken when the frame's scene starts (mrp_world_begin_frame_async, the host
    // thread): the same values as the host thread had at its end, the build may run on the scene's worker (RAC1_MRPW_BUILD_THREAD)
    struct BuildInputs {
        float vp[16] = {};    // mrp_world_view_proj
        float widen = 1.0f;   // mrp_lod_frame_widen
        int projection = 0;   // g_mrp_world_projection
        int modern_shadows = 0;
        bool sky = true, tfrag = true, tie = true, shrub = true;  // g_render_sky / tfrag / tie / shrub
    } in;
    bool built_on_worker = false;  // (build_triangles ran on the worker after the scene)
    double ms_build = 0;           // (its time)
};
Frame g_frame;

// orig: (host) the level's tfrag / tie / shrub textures (textures_<kind>.mrpt) to RGBA (palette[CSM1 index], the GS's 0..0x80
// alpha kept) in one texture array per size, every mip
void load_textures(Level& L) {
    static const char* names[3] = {"tfrag", "tie", "shrub"};
    std::map<std::pair<uint32_t, uint32_t>, int> by_size;
    for (int k = 0; k < 3; ++k) {
        const std::string path = asset_root() + "/" + level_dir_name(L.id) + "/textures_" + names[k] + ".mrpt";
        if (!mrp::read_textures(path, L.tex[k])) std::fprintf(stderr, "[mrpw] no %s textures %s\n", names[k], path.c_str());
        L.ref[k].assign(L.tex[k].size(), TexRef{});
        for (size_t i = 0; i < L.tex[k].size(); ++i) {
            const mrp::Texture& t = L.tex[k][i];
            if (t.width == 0 || t.height == 0 || t.mips.empty() || t.mips[0].size() < static_cast<size_t>(t.width) * t.height) continue;
            auto [it, fresh] = by_size.insert({{t.width, t.height}, static_cast<int>(L.buckets.size())});
            if (fresh) {
                Bucket b;
                b.w = t.width;
                b.h = t.height;
                L.buckets.push_back(b);
            }
            Bucket& b = L.buckets[static_cast<size_t>(it->second)];
            L.ref[k][i].bucket = it->second;
            L.ref[k][i].layer = static_cast<uint32_t>(b.textures.size());
            L.ref[k][i].mips = static_cast<uint32_t>(std::min<size_t>(t.mips.size(), 4));
            b.textures.push_back({k, static_cast<int>(i)});
            b.levels = std::max<uint32_t>(b.levels, L.ref[k][i].mips);
        }
    }
    auto csm1 = [](uint32_t i) { return ((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i; };
    std::vector<uint32_t> px;
    for (Bucket& b : L.buckets) {
        uint32_t maxl = 1;
        while ((b.w >> maxl) > 0 && (b.h >> maxl) > 0 && maxl < b.levels) ++maxl;
        b.levels = std::max(1u, std::min(b.levels, maxl));
        const uint32_t layers = static_cast<uint32_t>(b.textures.size());
        if (vk::is_active()) {
            vk::create_image(b.w, b.h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT, b.vk_tex, layers, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY, b.levels);
        } else {
            glGenTextures(1, &b.tex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &b.tex, "MRP world: texture arrays");
            glBindTexture(GL_TEXTURE_2D_ARRAY, b.tex);
            for (uint32_t l = 0; l < b.levels; ++l)
                glTexImage3D(GL_TEXTURE_2D_ARRAY, static_cast<GLint>(l), GL_RGBA8, static_cast<GLsizei>(std::max(1u, b.w >> l)), static_cast<GLsizei>(std::max(1u, b.h >> l)),
                             static_cast<GLsizei>(layers), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(b.levels) - 1);
        }
        for (size_t layer = 0; layer < b.textures.size(); ++layer) {
            const mrp::Texture& t = L.tex[b.textures[layer].first][static_cast<size_t>(b.textures[layer].second)];
            uint32_t pal[256];
            for (uint32_t i = 0; i < 256; ++i) pal[i] = t.palette[csm1(i)];
            for (uint32_t l = 0; l < b.levels; ++l) {
                const uint32_t w = std::max(1u, b.w >> l), h = std::max(1u, b.h >> l);
                px.assign(static_cast<size_t>(w) * h, 0u);
                if (l < t.mips.size() && t.mips[l].size() >= static_cast<size_t>(w) * h)
                    for (size_t q = 0; q < px.size(); ++q) px[q] = pal[t.mips[l][q]];
                if (vk::is_active()) {
                    vk::upload_image_2d(b.vk_tex, px.data(), px.size() * 4, 0, 0, static_cast<int>(w), static_cast<int>(h), 0,
                                        static_cast<uint32_t>(layer), l);
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
    if (vk::is_active()) {
        for (Bucket& b : L.buckets) vk::destroy_image(b.vk_tex);
        vk::destroy_buffer(L.vk_tvbo);
        vk::destroy_buffer(L.vk_tibo);
    } else {
        for (Bucket& b : L.buckets)
            if (b.tex != 0) glDeleteTextures(1, &b.tex);
        if (L.vao != 0) glDeleteVertexArrays(1, &L.vao);
        if (L.vbo != 0) glDeleteBuffers(1, &L.vbo);
        if (L.ibo != 0) glDeleteBuffers(1, &L.ibo);
        if (L.tvao != 0) glDeleteVertexArrays(1, &L.tvao);
        if (L.tvbo != 0) glDeleteBuffers(1, &L.tvbo);
        if (L.tibo != 0) glDeleteBuffers(1, &L.tibo);
    }
    L = Level{};
}

// orig: (host) a packet's kick program as GPU triangles (the GS's primitive assembly, as GsFront below: PRE / PRIM restart the
// primitive, strips / fans / lists; the world's A programs send no ADC)
template <typename P, typename F>
void build_tpl(Level& L, const P& p, F&& tvert) {
    Tpl t;
    t.first_vertex = static_cast<uint32_t>(L.tverts.size());
    t.first_index = static_cast<uint32_t>(L.tidx.size());
    uint64_t prim = 0;
    int slot = -1;
    std::vector<uint32_t> queue;
    uint32_t nv = 0;
    for (const Op& o : p.ops) {
        if (o.kind == OpKind::Tag) {
            if (((o.tag[0] >> 46) & 1) != 0) {
                prim = (o.tag[0] >> 47) & 0x7ff;
                t.has_prim = true;
                queue.clear();
            }
        } else if (o.kind == OpKind::Texture) {
            slot = o.index;
            t.last_slot = slot;
        } else if (o.kind == OpKind::Vertex) {
            L.tverts.push_back(tvert(o.index));
            queue.push_back(nv++);
            const int type = static_cast<int>(prim & 7);
            if (type != 3 && type != 4 && type != 5) {
                queue.clear();
                continue;
            }
            if (queue.size() < 3) continue;
            const uint32_t a = type == 5 ? queue.front() : queue[queue.size() - 3], b = queue[queue.size() - 2], c = queue.back();
            const uint32_t rel = static_cast<uint32_t>(L.tidx.size()) - t.first_index;
            if (t.runs.empty() || t.runs.back().slot != slot || t.runs.back().prim != prim) t.runs.push_back(TplRun{rel, 0, slot, prim});
            L.tidx.insert(L.tidx.end(), {a, b, c});
            t.runs.back().count += 3;
            if (type == 3) queue.clear();
            else if (type == 4) queue.erase(queue.begin());
            else queue.erase(queue.begin() + 1);
        }
    }
    t.last_prim = prim;
    L.tpl[&p] = std::move(t);
}

void build_templates(Level& L) {
    for (const TieClass& c : L.world->ties)
        for (const TiePacket& p : c.packets)
            build_tpl(L, p, [&p](uint16_t vi) {
                const TieKickVertex& v = p.verts[vi];
                TVert t{};
                const uint32_t nd = p.n_dinky;
                if (v.pos < nd) {
                    for (int l = 0; l < 3; ++l) t.pos[l] = p.dinky[v.pos][static_cast<size_t>(l)];
                } else {
                    const auto& f = p.fat[v.pos - nd];
                    for (int l = 0; l < 3; ++l) {
                        t.pos[l] = f[static_cast<size_t>(4 + l)];
                        t.delta[l] = f[static_cast<size_t>(l)];
                    }
                    t.pos[3] = 1;
                }
                for (int l = 0; l < 3; ++l) t.uv[l] = v.uv < nd ? p.dinky[v.uv][static_cast<size_t>(4 + l)] : p.fat[v.uv - nd][static_cast<size_t>(8 + l)];
                t.col[0] = static_cast<uint8_t>(v.colour & 63);
                t.col[1] = static_cast<uint8_t>(v.colour2[0] & 63);
                t.col[2] = static_cast<uint8_t>(v.colour2[1] & 63);
                t.col[3] = v.fat;
                if (v.fat != 0) t.pos[3] |= 2;
                return t;
            });
    for (const ShrubClass& c : L.world->shrubs)
        for (const ShrubPacket& p : c.packets)
            build_tpl(L, p, [&p](uint16_t vi) {
                const ShrubKickVertex& v = p.verts[vi];
                TVert t{};
                for (int l = 0; l < 3; ++l) {
                    t.pos[l] = p.pos[v.pos][static_cast<size_t>(l)];
                    t.uv[l] = p.uv[v.uv][static_cast<size_t>(l)];
                }
                t.col[0] = static_cast<uint8_t>(v.colour < 24 ? v.colour : 0);
                return t;
            });
}

// orig: (host) the level's world asset, its index, its textures, the GL buffers
void load_level(int id) {
    free_level(g_level);
    Level& L = g_level;
    L.id = id;
    L.tried = true;
    const double t0 = now_ms();
    L.world = std::make_unique<World>();
    if (!load_world(asset_root(), id, *L.world)) {
        std::fprintf(stderr, "[mrpw] no MRP world asset for level %d (%s/%s/world.mrpw): run setup.sh\n", id, asset_root().c_str(), level_dir_name(id).c_str());
        L.world.reset();
        return;
    }
    L.idx = std::make_unique<WorldIndex>();
    L.idx->add(L.world.get());
    load_textures(L);
    // the GPU path's templates (static)
    build_templates(L);
    if (vk::is_active()) {
        const size_t vbytes = std::max<size_t>(L.tverts.size() * sizeof(TVert), sizeof(TVert));
        TVert dummy_v{};
        vk::upload_buffer(L.vk_tvbo, L.tverts.empty() ? &dummy_v : L.tverts.data(), vbytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        const size_t ibytes = std::max<size_t>(L.tidx.size() * sizeof(uint32_t), sizeof(uint32_t));
        uint32_t dummy_i = 0;
        vk::upload_buffer(L.vk_tibo, L.tidx.empty() ? &dummy_i : L.tidx.data(), ibytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    } else {
        glGenVertexArrays(1, &L.vao);
        glGenBuffers(1, &L.vbo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &L.vbo, "MRP world: meshes");
        glGenBuffers(1, &L.ibo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &L.ibo, "MRP world: meshes");
        glBindVertexArray(L.vao);
        glBindBuffer(GL_ARRAY_BUFFER, L.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, L.ibo);
        const GLsizei S = sizeof(GpuV);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, wp)));
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, vu)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, stq)));
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, lodq)));
        glVertexAttribIPointer(4, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(GpuV, rgba)));
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, fog)));
        glVertexAttribIPointer(6, 2, GL_UNSIGNED_INT, S, reinterpret_cast<void*>(offsetof(GpuV, word)));
        glVertexAttribIPointer(7, 2, GL_UNSIGNED_INT, S, reinterpret_cast<void*>(offsetof(GpuV, state)));
        for (GLuint a = 0; a < 8; ++a) glEnableVertexAttribArray(a);
        glGenVertexArrays(1, &L.tvao);
        glGenBuffers(1, &L.tvbo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &L.tvbo, "MRP world: meshes");
        glGenBuffers(1, &L.tibo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &L.tibo, "MRP world: meshes");
        glBindVertexArray(L.tvao);
        glBindBuffer(GL_ARRAY_BUFFER, L.tvbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(L.tverts.size() * sizeof(TVert)), L.tverts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, L.tibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(L.tidx.size() * sizeof(uint32_t)), L.tidx.data(), GL_STATIC_DRAW);
        const GLsizei T = sizeof(TVert);
        glVertexAttribIPointer(0, 4, GL_SHORT, T, reinterpret_cast<void*>(offsetof(TVert, pos)));
        glVertexAttribIPointer(1, 4, GL_SHORT, T, reinterpret_cast<void*>(offsetof(TVert, delta)));
        glVertexAttribIPointer(2, 4, GL_SHORT, T, reinterpret_cast<void*>(offsetof(TVert, uv)));
        glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, T, reinterpret_cast<void*>(offsetof(TVert, col)));
        for (GLuint a = 0; a < 4; ++a) glEnableVertexAttribArray(a);
        draw_ids_reserve(0);  // (the draw's index: g_draw_ids)
        glVertexAttribIPointer(8, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);
        glVertexAttribDivisor(8, 1);
        glEnableVertexAttribArray(8);
        glBindVertexArray(0);
    }
    L.loaded = true;
    static_level_build(g_slevel, *L.world);  // (the classes as the passes find them in EE memory)
    tfrag_level_build(g_tlevel, *L.world);   // (the tfrag images / plans from EE memory)
    g_tcols = TfragColours{};
    L.reset_walker = true;
    size_t ns = 0, nt = 0;
    for (const auto& c : L.world->shrubs) ns += c.packets.size();
    for (const auto& c : L.world->ties) nt += c.packets.size();
    std::printf("[mrpw] level %d: %zu shrub classes (%zu packets), %zu tie classes (%zu packets), %zu tfrags; %zu / %zu / %zu textures in %zu arrays (%.0f ms)\n", id,
                L.world->shrubs.size(), ns, L.world->ties.size(), nt, L.world->tfrags.size(), L.tex[0].size(), L.tex[1].size(), L.tex[2].size(), L.buckets.size(), now_ms() - t0);
}

// ---- shaders -----------------------------------------------------------------------------------

const char* kVs = R"(#version 460 core
layout(location = 0) in vec4 aWorld;
layout(location = 1) in vec4 aVu;
layout(location = 2) in vec3 aStq;
layout(location = 3) in float aLodQ;
layout(location = 4) in uvec4 aRgba;
layout(location = 5) in float aFog;
layout(location = 6) in uvec2 aTex;    // the texture word, the array layer
layout(location = 7) in uvec2 aState;  // the GS state: draw_flags, FOGCOL
uniform int uMode;      // 0 the world renderers' projection (uVP), 1 the GS's frame
uniform mat4 uVP;
uniform vec4 uFrame;    // fb width, fb height, x scale, 0
uniform vec4 uXyOff;    // XYOFFSET x, y (1/16 px), depth a, depth b
)" RAC1_SHADOW_VS R"(
smooth out vec3 vStq;
noperspective out float vLodQ;
noperspective out vec4 vColor;
flat out vec4 vFlat;
noperspective out float vFog;
flat out uvec2 vTex;
flat out uvec2 vState;
void main() {
    vTex = aTex;
    vState = aState;
    if (uMode == 0) {
        gl_Position = sh_clip(uVP * aWorld);
    } else {
        vShWorld = vec3(0.0);  // the GS's frame: x = X - XYOFFSET (px), depth a + b Z, as homogeneous coordinates with w (aVu = (X w, Y w, Z w, w))
        float w = aVu.w;
        float X1 = uFrame.x * 0.5;
        float sx = uFrame.z / X1, sy = 1.0 / (uFrame.y * 0.5);
        vec2 c = vec2(uXyOff.x / 16.0 + X1, uXyOff.y / 16.0 + uFrame.y * 0.5);
        float zc = 2.0 * (uXyOff.z * w + uXyOff.w * aVu.z) - w;
        gl_Position = vec4((aVu.x - c.x * w) * sx, -(aVu.y - c.y * w) * sy, zc, w);
    }
    vStq = aStq;
    vLodQ = aLodQ;
    vColor = vec4(aRgba) / 255.0;
    vFlat = vColor;
    vFog = aFog;
}
)";

// The GPU path (tie A, shrub A): the packet's kicked vertex (its sources: the template) evaluated as the CPU reference does
// (tie_kick / shrub_kick, src/formats/world_mrp.cpp) with the instance's rows, morph, lit colours, F
const char* kVsGpu = R"(#version 460 core
layout(location = 0) in ivec4 aPos;    // x, y, z, flags (1: a tie fat position, 2: a tie fat colour)
layout(location = 1) in ivec4 aDelta;  // a tie fat vertex's delta
layout(location = 2) in ivec4 aUv;     // s, t, q (shrub h) x 4096
layout(location = 3) in uvec4 aCol;    // colour indices (tie fat: c0, c1, c2)
layout(location = 8) in uint aDraw;    // the draw's index (its base instance: g_draw_ids)
struct Inst {
    mat4 clip;
    vec4 rows[4];
    vec4 morph;
    uvec4 info;  // colour first, F, alpha, kind (0 tie, 1 shrub)
};
layout(std430, binding = 0) readonly buffer Insts { Inst insts[]; };
layout(std430, binding = 1) readonly buffer Cols { uvec4 cols[]; };
layout(std430, binding = 2) readonly buffer Cmds { uvec4 cmds[]; };  // instance, texture word, layer, 0
layout(std430, binding = 3) readonly buffer RCols { uint rcols[]; };  // the resident lit colours: RGBA8 as the level's data holds them
uniform int uMode;
uniform vec4 uFrame;
uniform vec4 uXyOff;
)" RAC1_SHADOW_VS R"(
smooth out vec3 vStq;
noperspective out float vLodQ;
noperspective out vec4 vColor;
flat out vec4 vFlat;
noperspective out float vFog;
flat out uvec2 vTex;
flat out uvec2 vState;
int ftoi4(float f) { return int(clamp(f * 16.0, -2147483648.0, 2147483520.0)); }
void main() {
    uvec4 cmd = cmds[2u * aDraw];
    vState = uvec2(cmd.w, cmds[2u * aDraw + 1u].x);
    Inst I = insts[cmd.x];
    vTex = cmd.yz;
    // the position: tie fat (delta x morph) + base
    vec3 P = vec3(aPos.xyz);
    if ((aPos.w & 1) != 0) {
        precise vec3 d = vec3(aDelta.xyz) * I.morph.x;
        P = d + vec3(aPos.xyz);
    }
    precise vec4 p = I.rows[3];
    p = p + I.rows[0] * P.x;
    p = p + I.rows[1] * P.y;
    p = p + I.rows[2] * P.z;
    float Q = 1.0 / p.w;
    vec3 uv = vec3(aUv.xyz) * (1.0 / 4096.0);
    // the colour: tie dinky the lit colour's bytes, tie fat ((C1 0.5 + C2 0.5) morph.w + C0 morph.z), shrub the colour and the instance's alpha
    uvec4 c;
    uint first = I.info.x;
    uint kind = I.info.w & 1u;
    bool resident = (I.info.w & 2u) != 0u;
    if (kind == 0u) {
        // (a resident colour is the byte VU1 memory holds as 2^23 + byte after the UNPACK's STMOD row 0x4b000000)
        uvec4 c0 = resident ? (uvec4(rcols[first + aCol.x]) >> uvec4(0u, 8u, 16u, 24u) & 0xffu) | 0x4b000000u : cols[first + aCol.x];
        if ((aPos.w & 2) != 0) {
            uvec4 c1 = resident ? (uvec4(rcols[first + aCol.y]) >> uvec4(0u, 8u, 16u, 24u) & 0xffu) | 0x4b000000u : cols[first + aCol.y];
            uvec4 c2 = resident ? (uvec4(rcols[first + aCol.z]) >> uvec4(0u, 8u, 16u, 24u) & 0xffu) | 0x4b000000u : cols[first + aCol.z];
            for (int l = 0; l < 4; ++l) {
                precise float a = uintBitsToFloat(c1[l]) * 0.5;
                a = a + uintBitsToFloat(c2[l]) * 0.5;
                precise float b = uintBitsToFloat(c0[l]) * I.morph.z;
                precise float r = a * I.morph.w;
                r = r + b;
                c[l] = floatBitsToUint(r) & 0xffu;
            }
        } else {
            c = c0 & 0xffu;
        }
    } else if (resident) {
        c = uvec4(uvec3(rcols[first + aCol.x]) >> uvec3(0u, 8u, 16u) & 0xffu, I.info.z);
    } else {
        c = uvec4(cols[first + aCol.x].xyz & 0xffu, I.info.z);
    }
    vColor = vec4(c) / 255.0;
    vFlat = vColor;
    vFog = float(I.info.y);
    vStq = uv;  // ((s, t, q) Q: undivided by Q = 1 / w)
    vLodQ = uv.z * Q;
    if (uMode == 0) {
        gl_Position = sh_clip(I.clip * vec4(P, 1.0));
    } else {
        vShWorld = vec3(0.0);  // the GS's frame from the kicked coordinates (FTOI4, 16 bits; Z 24 bits)
        float w = p.w;
        float X = float(ftoi4(p.x * Q) & 0xffff) / 16.0, Y = float(ftoi4(p.y * Q) & 0xffff) / 16.0;
        float Z = float((ftoi4(p.z * Q) >> 4) & 0xffffff);
        float X1 = uFrame.x * 0.5;
        float sx = uFrame.z / X1, sy = 1.0 / (uFrame.y * 0.5);
        vec2 cc = vec2(uXyOff.x / 16.0 + X1, uXyOff.y / 16.0 + uFrame.y * 0.5);
        float zc = (2.0 * (uXyOff.z + uXyOff.w * Z) - 1.0) * w;
        gl_Position = vec4((X - cc.x) * w * sx, -(Y - cc.y) * w * sy, zc, w);
    }
}
)";

// the GS's texture functions, alpha test (its AFAIL modes by two passes: uAtPass 1 the pixels that pass, 2 those that fail),
// the GS's mip level from Q (TEX1 LCM / L / K, MXL) or the GPU's over every mip, the fog (as the MRP mobys')
const char* kFs = R"(#version 460 core
#extension GL_KHR_shader_subgroup_ballot : require
smooth in vec3 vStq;
noperspective in float vLodQ;
noperspective in vec4 vColor;
flat in vec4 vFlat;
noperspective in float vFog;
flat in uvec2 vTex;
flat in uvec2 vState;  // the GS state as data: x = TME | IIP << 1 | FGE << 2 | the alpha test's mode << 3 | ATST << 4 | AREF << 8 | texture unit << 16, y = FOGCOL
uniform sampler2DArray uTexs[16];
uniform int uAtPass, uMipMode, uShade, uFge;
uniform vec3 uFogCol;  // (FOGCOL: per batch, RKey; as a uniform the fog's arithmetic compiles as before bit for bit)
)" RAC1_SHADOW_FS R"(
out vec4 FragColor;
void main() {
    sh_prepare();
    uint fl = vState.x;
    int uTme = int(fl & 1u), uIip = int((fl >> 1) & 1u), uAte = int((fl >> 3) & 1u), uAtst = int((fl >> 4) & 7u), uAref = int((fl >> 8) & 0xffu);
    uint unit = (fl >> 16) & 15u;  // (the CPU path's 0, the GPU path's from its draw command: one per primitive, not per wave)
#ifdef SH_POINTS
    if (uShPass != 0 && uAte == 0) return;  // (a caster without the alpha test: its depth alone, no texture read)
#endif
    vec4 c = uIip != 0 ? vColor : vFlat;
#ifdef SH_POINTS
    vec3 c_game = c.rgb;  // (the game's colour: an emissive texel's, sh_emissive)
#endif
    c.rgb = sh_apply(c.rgb);
#ifdef SH_POINTS
    c.rgb = sh_points(c.rgb);  // (the game's point lights per pixel; the lighting's variant only)
#endif
    vec4 o = c;
    uint w = vTex.x;
    float uLayer = float(vTex.y);
    if (uTme != 0) {
        vec2 st = vStq.xy / vStq.z;
#ifdef SH_POINTS
        st = sh_mat_uv(st, (w >> 25) & 31u);  // (lava, mud, quicksand: the slow flow)
#endif
        vec4 t;
        if ((w & 1u) != 0u) {
            t = vec4(128.0 / 255.0);
        } else {
            int mips = int((w >> 22) & 7u) + 1;
            // (a multi-draw's fragments of several draws share a wave, their units may differ: each unit the wave holds
            // sampled in turn with an index uniform over the subgroup; a quad is one primitive's, so its derivatives stay its own)
            for (;;) {
                uint u = subgroupBroadcastFirst(unit);
                if (u != unit) continue;
                if (uMipMode == 0) {
                    int mxl = int((w >> 4) & 7u), lcm = int((w >> 7) & 1u), L = int((w >> 8) & 3u);
                    float K = float((int(w >> 10) << 20) >> 20) / 16.0;
                    float lod = lcm != 0 ? K : log2(1.0 / max(abs(vLodQ), 1e-20)) * float(1 << L) + K;
                    int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), mxl);
                    level = min(level, mips - 1);
                    t = textureLod(uTexs[u], vec3(st, uLayer), float(level));
                } else {
                    float lod = clamp(textureQueryLod(uTexs[u], st).y, 0.0, float(mips - 1));
                    t = textureLod(uTexs[u], vec3(st, uLayer), lod);
                }
                break;
            }
        }
        uint tfx = (w >> 1) & 3u, tcc = (w >> 3) & 1u;
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
#ifdef SH_POINTS
        o.rgb = sh_emissive(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, (w >> 25) & 31u);  // (the word's bits 25..29)
        o.rgb = sh_material(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, tfx == 1u ? vec3(1.0) : c.rgb * (255.0 / 128.0), (w >> 25) & 31u);
#endif
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    else if (uShade == 3) o.rgb = vec3(0.8);
    o = clamp(o, 0.0, 1.0);
    if (uFge != 0) {
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
        if (uShPass != 0 && !pass) discard;  // (the shadow map: the cut-outs' passing pixels cast)
        if (uAtPass == 1 && !pass) discard;  // (uAte: the draw's mode - the test on and its failing pixels treated apart)
        if (uAtPass == 2 && pass) discard;
    }
    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
)";

// orig: (host) the modern lighting's second pass of an alpha-tested batch (AFAIL RGB_ONLY: its failing pixels, no depth written)
// tested LESS where the GS's test is GEQUAL (the host's LEQUAL): the pixels where the batch's first pass wrote its own depth -
// those the second pass discards (their alpha passes) - not shaded again, the driver's early depth test leaving them out. Only a
// failing pixel lying exactly on another surface's depth is lost (a few pixels in a million: docs/performance.md). 0: LEQUAL, as
// the original lighting keeps it (RAC1_MRPW_P2_LESS)
const bool g_p2_less = [] {
    const char* e = std::getenv("RAC1_MRPW_P2_LESS");
    return e == nullptr || std::atoi(e) != 0;
}();
GLuint g_prog = 0, g_prog_gpu = 0;
GLuint g_prog_l = 0, g_prog_gpu_l = 0;  // the modern lighting's variants (SH_POINTS)
GLuint g_prog_e = 0, g_prog_gpu_e = 0;  // the shadow pass's depth-only variants (early fragment tests: the opaque batches)
GLuint g_ssbo[3] = {}, g_indirect = 0;
// the resident lit colours (mrp_world_static.h): the mirror the worker keeps, its GL buffer
ResidentColours g_rcols;
GLuint g_rcol_buf = 0;
size_t g_rcol_size = 0;
const bool g_static_on = [] {
    const char* e = std::getenv("RAC1_MRPW_STATIC");
    return e == nullptr || std::atoi(e) != 0;
}();
GLuint g_samplers[2][16] = {};
using PfnMultiDrawElementsIndirect = void(APIENTRYP)(GLenum, GLenum, const void*, GLsizei, GLsizei);
PfnMultiDrawElementsIndirect p_multi_draw_elements_indirect = nullptr;
constexpr GLenum kGlShaderStorageBuffer = 0x90D2, kGlDrawIndirectBuffer = 0x8F3F;
struct Loc {
    GLint mode, vp, frame, xyoff, tex, word, layer, tme, iip, ate, atst, aref, atpass, mip, shade, fge, fogcol, u8;
    ShadowLocs sh;  // (the modern lighting, mrp_lighting.h)
} g_loc{}, g_loc_gpu{}, g_loc_l{}, g_loc_gpu_l{}, g_loc_e{}, g_loc_gpu_e{};
// orig: (host) the draw's programs swapped for the lighting's variants while it is on (restored at the scope's end)
struct VariantScope {
    bool on = false;
    explicit VariantScope(bool want) {
        if (want && g_prog_l != 0 && g_prog_gpu_l != 0) {
            on = true;
            swap_all();
        }
    }
    ~VariantScope() {
        if (on) swap_all();
    }
    static void swap_all() {
        std::swap(g_prog, g_prog_l);
        std::swap(g_prog_gpu, g_prog_gpu_l);
        std::swap(g_loc, g_loc_l);
        std::swap(g_loc_gpu, g_loc_gpu_l);
    }
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
        std::fprintf(stderr, "[mrpw] shader: %s\n", log);
    }
    return s;
}

// (`variant` the modern lighting's program, SH_POINTS defined: its point lights; the original program's code - its
// rounding - left as it was)
GLuint link(const char* vsrc, bool variant = false, bool early = false) {
    auto src = [variant, early](const char* s) {
        std::string t = s;
        const std::string v = "#version 460 core\n";
        if (variant && t.compare(0, v.size(), v) == 0) t.insert(v.size(), "#define SH_POINTS 1\n");
        // (the shadow pass's depth-only variant: the depth test and write before the fragment shader - an opaque caster's fragments
        // hidden by nearer ones not run; its draws have no alpha test, the shader leaves at once)
        if (early && s == kFs) t.insert(t.find("smooth in vec3 vStq"), "layout(early_fragment_tests) in;\n");
        return t;
    };
    const std::string vss = src(vsrc), fss = src(kFs);
    const GLuint vs = compile(GL_VERTEX_SHADER, vss.c_str()), fs = compile(GL_FRAGMENT_SHADER, fss.c_str());
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrpw] link: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}
Loc locs(GLuint prog) {
    Loc l;
    l.mode = glGetUniformLocation(prog, "uMode");
    l.vp = glGetUniformLocation(prog, "uVP");
    l.frame = glGetUniformLocation(prog, "uFrame");
    l.xyoff = glGetUniformLocation(prog, "uXyOff");
    l.tex = glGetUniformLocation(prog, "uTexs");
    l.word = glGetUniformLocation(prog, "uWord");
    l.layer = glGetUniformLocation(prog, "uLayer");
    l.tme = glGetUniformLocation(prog, "uTme");
    l.iip = glGetUniformLocation(prog, "uIip");
    l.ate = glGetUniformLocation(prog, "uAte");
    l.atst = glGetUniformLocation(prog, "uAtst");
    l.aref = glGetUniformLocation(prog, "uAref");
    l.atpass = glGetUniformLocation(prog, "uAtPass");
    l.mip = glGetUniformLocation(prog, "uMipMode");
    l.shade = glGetUniformLocation(prog, "uShade");
    l.fge = glGetUniformLocation(prog, "uFge");
    l.fogcol = glGetUniformLocation(prog, "uFogCol");
    l.u8 = glGetUniformLocation(prog, "uU8");
    l.sh = mrp_lighting_locs(prog);
    return l;
}

struct WorldVkPush {
    float uVP[16];
    float uFrame[4];
    float uXyOff[4];
    float uFogCol_Pad[4];
    int32_t uMode;
    int32_t uMipMode;
    int32_t uShade;
    int32_t uFge;
    int32_t uAtPass;
    int32_t uU8;
    int32_t _pad0 = 0;
    int32_t _pad1 = 0;
};
static_assert(sizeof(WorldVkPush) == 144);

struct WorldVkPipeKey {
    bool gpu = false;
    bool lit = false;
    bool early = false;
    bool caster = false;
    bool depth_clamp = false;
    bool depth_test = false;
    bool depth_write = false;
    VkCompareOp depth_compare = VK_COMPARE_OP_ALWAYS;
    uint8_t color_mask = 0xF;
    uint64_t prim = 0;
    uint64_t alpha = 0;
    VkFormat color_fmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_fmt = VK_FORMAT_D32_SFLOAT_S8_UINT;
    bool has_stencil = true;
    bool operator==(const WorldVkPipeKey& o) const {
        return gpu == o.gpu && lit == o.lit && early == o.early && caster == o.caster &&
               depth_clamp == o.depth_clamp && depth_test == o.depth_test && depth_write == o.depth_write &&
               depth_compare == o.depth_compare && color_mask == o.color_mask &&
               prim == o.prim && alpha == o.alpha && color_fmt == o.color_fmt &&
               depth_fmt == o.depth_fmt && has_stencil == o.has_stencil;
    }
};

struct WorldVkPipeKeyHash {
    size_t operator()(const WorldVkPipeKey& k) const {
        size_t h = std::hash<uint64_t>{}(k.prim ^ (k.alpha * 0x9e3779b97f4a7c15ULL));
        uint32_t flags = (k.gpu ? 1u : 0u) | (k.lit ? 2u : 0u) | (k.early ? 4u : 0u) |
                         (k.caster ? 8u : 0u) | (k.depth_clamp ? 16u : 0u) |
                         (k.depth_test ? 32u : 0u) | (k.depth_write ? 64u : 0u) |
                         (static_cast<uint32_t>(k.depth_compare) << 7) |
                         (static_cast<uint32_t>(k.color_mask) << 11) |
                         (static_cast<uint32_t>(k.color_fmt) << 15) |
                         (static_cast<uint32_t>(k.depth_fmt) << 23) |
                         (k.has_stencil ? (1u << 31) : 0u);
        return h ^ (std::hash<uint32_t>{}(flags) + 0x9e3779b9 + (h << 6) + (h >> 2));
    }
};

struct VkWorldState {
    bool inited = false;
    VkDescriptorSetLayout set0_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipe_layout = VK_NULL_HANDLE;
    VkShaderModule vs = VK_NULL_HANDLE;
    VkShaderModule vs_gpu = VK_NULL_HANDLE;
    VkShaderModule fs = VK_NULL_HANDLE;
    VkShaderModule vs_lit = VK_NULL_HANDLE;
    VkShaderModule vs_gpu_lit = VK_NULL_HANDLE;
    VkShaderModule fs_lit = VK_NULL_HANDLE;
    VkShaderModule fs_early = VK_NULL_HANDLE;
    VkSampler samplers[2][16]{};
    std::unordered_map<WorldVkPipeKey, VkPipeline, WorldVkPipeKeyHash> pipelines;
    vk::RingAlloc cpu_vbo_alloc{};
    vk::RingAlloc cpu_ibo_alloc{};
    vk::RingAlloc insts_alloc{};
    vk::RingAlloc cols_alloc{};
    vk::RingAlloc cmds_alloc{};
    vk::RingAlloc rcols_alloc{};
    vk::RingAlloc indirect_alloc{};
} g_vk_world;

void shutdown_vk_world() {
    VkDevice dev = vk::device();
    if (dev != VK_NULL_HANDLE) {
        for (auto& kv : g_vk_world.pipelines) {
            if (kv.second != VK_NULL_HANDLE) vkDestroyPipeline(dev, kv.second, nullptr);
        }
        g_vk_world.pipelines.clear();
        auto destroy_mod = [&](VkShaderModule& m) {
            if (m != VK_NULL_HANDLE) {
                vkDestroyShaderModule(dev, m, nullptr);
                m = VK_NULL_HANDLE;
            }
        };
        destroy_mod(g_vk_world.vs);
        destroy_mod(g_vk_world.vs_gpu);
        destroy_mod(g_vk_world.fs);
        destroy_mod(g_vk_world.vs_lit);
        destroy_mod(g_vk_world.vs_gpu_lit);
        destroy_mod(g_vk_world.fs_lit);
        destroy_mod(g_vk_world.fs_early);
        if (g_vk_world.pipe_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(dev, g_vk_world.pipe_layout, nullptr);
            g_vk_world.pipe_layout = VK_NULL_HANDLE;
        }
        if (g_vk_world.set0_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(dev, g_vk_world.set0_layout, nullptr);
            g_vk_world.set0_layout = VK_NULL_HANDLE;
        }
    }
    free_level(g_level);
    g_vk_world = VkWorldState{};
}

bool init_vk_world() {
    if (g_vk_world.inited) return true;
    VkDevice dev = vk::device();
    if (dev == VK_NULL_HANDLE) return false;

    VkDescriptorSetLayoutBinding b[5]{};
    for (uint32_t i = 0; i < 4; ++i) {
        b[i].binding = i;
        b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    b[4].binding = 4;
    b[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b[4].descriptorCount = 16;
    b[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo slci{};
    slci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    slci.bindingCount = 5;
    slci.pBindings = b;
    if (vkCreateDescriptorSetLayout(dev, &slci, nullptr, &g_vk_world.set0_layout) != VK_SUCCESS) return false;

    VkDescriptorSetLayout set_layouts[2] = {
        g_vk_world.set0_layout,
        reinterpret_cast<VkDescriptorSetLayout>(mrp_lighting_vk_set_layout()),
    };
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(WorldVkPush);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 2;
    plci.pSetLayouts = set_layouts;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &g_vk_world.pipe_layout) != VK_SUCCESS) return false;

    g_vk_world.vs = vk::create_shader_module(vk::shaders::kMrpWorldVs, sizeof(vk::shaders::kMrpWorldVs));
    g_vk_world.vs_gpu = vk::create_shader_module(vk::shaders::kMrpWorldGpuVs, sizeof(vk::shaders::kMrpWorldGpuVs));
    g_vk_world.fs = vk::create_shader_module(vk::shaders::kMrpWorldFs, sizeof(vk::shaders::kMrpWorldFs));
    g_vk_world.vs_lit = vk::create_shader_module(vk::shaders::kMrpWorldLitVs, sizeof(vk::shaders::kMrpWorldLitVs));
    g_vk_world.vs_gpu_lit = vk::create_shader_module(vk::shaders::kMrpWorldGpuLitVs, sizeof(vk::shaders::kMrpWorldGpuLitVs));
    g_vk_world.fs_lit = vk::create_shader_module(vk::shaders::kMrpWorldLitFs, sizeof(vk::shaders::kMrpWorldLitFs));
    g_vk_world.fs_early = vk::create_shader_module(vk::shaders::kMrpWorldEarlyFs, sizeof(vk::shaders::kMrpWorldEarlyFs));

    for (int mm = 0; mm < 2; ++mm) {
        for (uint32_t s = 0; s < 16; ++s) {
            const VkFilter mag = (s & 1) != 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            const VkFilter min = (mm != 0 || (s & 2) != 0) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            const VkSamplerMipmapMode mip = mm == 0 ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
            const VkSamplerAddressMode ws = (s & 4) != 0 ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            const VkSamplerAddressMode wt = (s & 8) != 0 ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            g_vk_world.samplers[mm][s] = vk::get_sampler(min, mag, ws, wt, false, VK_COMPARE_OP_ALWAYS, mm != 0 ? 16.0f : 1.0f, mip, 16.0f);
        }
    }
    vk::register_shutdown_callback(shutdown_vk_world);
    g_vk_world.inited = true;
    return true;
}

VkPipeline get_world_vk_pipeline(const WorldVkPipeKey& k) {
    auto it = g_vk_world.pipelines.find(k);
    if (it != g_vk_world.pipelines.end()) return it->second;

    vk::GraphicsPipelineDesc ci{};
    ci.layout = g_vk_world.pipe_layout;
    if (k.early) {
        ci.vs = k.gpu ? g_vk_world.vs_gpu_lit : g_vk_world.vs_lit;
        ci.fs = g_vk_world.fs_early;
    } else if (k.lit) {
        ci.vs = k.gpu ? g_vk_world.vs_gpu_lit : g_vk_world.vs_lit;
        ci.fs = g_vk_world.fs_lit;
    } else {
        ci.vs = k.gpu ? g_vk_world.vs_gpu : g_vk_world.vs;
        ci.fs = g_vk_world.fs;
    }

    if (k.gpu) {
        VkVertexInputBindingDescription bind{};
        bind.binding = 0;
        bind.stride = sizeof(TVert);
        bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        ci.vertex_bindings = {bind};
        ci.vertex_attribs = {
            {0, 0, VK_FORMAT_R16G16B16A16_SINT, static_cast<uint32_t>(offsetof(TVert, pos))},
            {1, 0, VK_FORMAT_R16G16B16A16_SINT, static_cast<uint32_t>(offsetof(TVert, delta))},
            {2, 0, VK_FORMAT_R16G16B16A16_SINT, static_cast<uint32_t>(offsetof(TVert, uv))},
            {3, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(TVert, col))},
        };
    } else {
        VkVertexInputBindingDescription bind{};
        bind.binding = 0;
        bind.stride = sizeof(GpuV);
        bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        ci.vertex_bindings = {bind};
        ci.vertex_attribs = {
            {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(GpuV, wp))},
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(GpuV, vu))},
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(GpuV, stq))},
            {3, 0, VK_FORMAT_R32_SFLOAT, static_cast<uint32_t>(offsetof(GpuV, lodq))},
            {4, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(GpuV, rgba))},
            {5, 0, VK_FORMAT_R32_SFLOAT, static_cast<uint32_t>(offsetof(GpuV, fog))},
            {6, 0, VK_FORMAT_R32G32_UINT, static_cast<uint32_t>(offsetof(GpuV, word))},
            {7, 0, VK_FORMAT_R32G32_UINT, static_cast<uint32_t>(offsetof(GpuV, state))},
        };
    }

    ci.cull_mode = VK_CULL_MODE_NONE;
    ci.depth_clamp = k.depth_clamp;
    ci.depth_test = k.depth_test;
    ci.depth_write = k.depth_write;
    ci.depth_compare = k.depth_compare;
    if (k.caster) {
        ci.depth_bias_enable = true;
        ci.depth_bias_constant = 2.0f;
        ci.depth_bias_slope = 1.5f;
    }
    ci.color_write_mask = k.color_mask;
    ci.color_format = k.color_fmt;
    ci.depth_format = k.depth_fmt;
    ci.has_depth_attachment = (k.depth_fmt != VK_FORMAT_UNDEFINED);
    ci.has_stencil_attachment = k.has_stencil;

    if (!k.caster) {
        gs::DrawState ds{};
        ds.prim = k.prim;
        ds.alpha = k.alpha;
        float fix = 0.0f;
        gs::vk_blend_state(ds, false, ci, fix);
    }

    VkPipeline p = vk::create_graphics_pipeline(ci);
    g_vk_world.pipelines[k] = p;
    return p;
}

bool init_gl() {
    if (vk::is_active()) return init_vk_world();
    if (g_prog != 0) return true;
    p_multi_draw_elements_indirect = reinterpret_cast<PfnMultiDrawElementsIndirect>(glfwGetProcAddress("glMultiDrawElementsIndirect"));
    if (p_multi_draw_elements_indirect == nullptr) return false;
    g_prog = link(kVs);
    g_prog_gpu = link(kVsGpu);
    if (g_prog == 0 || g_prog_gpu == 0) {
        g_prog = 0;
        return false;
    }
    g_loc = locs(g_prog);
    g_loc_gpu = locs(g_prog_gpu);
    g_prog_l = link(kVs, true);
    g_prog_gpu_l = link(kVsGpu, true);
    if (g_prog_l != 0 && g_prog_gpu_l != 0) {
        g_loc_l = locs(g_prog_l);
        g_loc_gpu_l = locs(g_prog_gpu_l);
    }
    g_prog_e = link(kVs, true, true);
    g_prog_gpu_e = link(kVsGpu, true, true);
    if (g_prog_e != 0 && g_prog_gpu_e != 0) {
        g_loc_e = locs(g_prog_e);
        g_loc_gpu_e = locs(g_prog_gpu_e);
    } else {
        g_prog_e = g_prog_gpu_e = 0;
    }
    glGenBuffers(3, g_ssbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 3, g_ssbo, "MRP world: frame buffers");
    glGenBuffers(1, &g_indirect);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_indirect, "MRP world: frame buffers");
    constexpr GLenum kGlMaxAnisotropy = 0x84FE;
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

// ---- the frame's GS stream to GPU triangles ------------------------------------------------------

uint32_t fld(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

// orig: (host) a draw's GS state as the fragment shader's data (see RKey): TME, IIP, FGE (and the fog switch), the alpha test's
// mode (on, not ALWAYS, its failing pixels not drawn as passing: the AFAIL FB_ONLY without a depth write is one pass), ATST, AREF, the
// texture unit
uint32_t draw_flags(const Key& k, uint32_t unit) {
    const uint32_t ate = fld(k.test, 0, 1), atst = fld(k.test, 1, 3), afail = fld(k.test, 12, 2);
    const bool zwrite = fld(k.zbuf, 32, 1) == 0;
    const bool z_effective = zwrite && fld(k.test, 16, 1) != 0 && fld(k.test, 17, 2) != 1;
    const uint32_t mode = ate != 0 && atst != 1 && !(afail == 1 && !z_effective) ? 1u : 0u;
    const uint32_t fge = g_mrp_world_fog != 0 && fld(k.prim, 5, 1) != 0 ? 1u : 0u;
    return fld(k.prim, 4, 1) | (fld(k.prim, 3, 1) << 1) | (fge << 2) | (mode << 3) | (atst << 4) | (fld(k.test, 4, 8) << 8) | (unit << 16);
}
// orig: (host) the batch key of a draw (RKey)
bool g_view_only_emit = false;  // the batches made now are the view's only (RKey::view_only)
RKey batch_key(const Key& k, bool gpu, bool sky) {
    RKey r;
    r.view_only = g_view_only_emit;
    r.gpu = gpu;
    r.sky = sky;
    r.abe = fld(k.prim, 6, 1);
    r.alpha = k.alpha;
    r.ztest = fld(k.test, 16, 3);
    r.zwrite = fld(k.zbuf, 32, 1) == 0 ? 1u : 0u;
    r.fogcol = k.fogcol | ((draw_flags(k, 0) & 4u) << 22);  // (FOGCOL and FGE: uniforms, see kFs)
    r.pass2 = (draw_flags(k, 0) & 8u) != 0 ? fld(k.test, 12, 2) : 0u;
    static const bool unit_key = std::getenv("RAC1_MRPW_UNITKEY") != nullptr;  // (testing: the GPU path's texture in the key too)
    if (!gpu || unit_key) {
        r.bucket = k.bucket;
        r.sampler = k.sampler;
    }
    return r;
}

// A texture the frame's build found first (the sky's, a world GIF stream's): made with the GL / Vulkan image at once on the
// host thread; on the scene's worker (the build there, RAC1_MRPW_BUILD_THREAD) its texels kept and the image made by the host
// thread when it takes the frame (mrp_world_finish_frame), its bucket's index reserved now (only the worker adds buckets meanwhile)
struct NewBucket {
    uint32_t w = 0, h = 0;
    std::vector<std::vector<uint32_t>> levels;  // RGBA, level 0 first
};
bool g_tex_defer = false;              // (the build runs on the scene's worker)
std::vector<NewBucket> g_new_buckets;  // (made by the host thread: create_new_buckets)
// orig: (host) the GPU image of a bucket of one layer, `levels` mip levels
void create_bucket_image(Bucket& b, const std::vector<std::vector<uint32_t>>& levels) {
    const uint32_t n = static_cast<uint32_t>(levels.size());
    if (vk::is_active()) {
        vk::create_image(b.w, b.h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                         VK_IMAGE_ASPECT_COLOR_BIT, b.vk_tex, 1, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY, n);
        for (uint32_t l = 0; l < n; ++l)
            vk::upload_image_2d(b.vk_tex, levels[l].data(), levels[l].size() * 4, 0, 0, static_cast<int>(b.w >> l), static_cast<int>(b.h >> l), 0, 0, l);
    } else {
        glGenTextures(1, &b.tex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &b.tex, "MRP world: texture arrays");
        glBindTexture(GL_TEXTURE_2D_ARRAY, b.tex);
        for (uint32_t l = 0; l < n; ++l)
            glTexImage3D(GL_TEXTURE_2D_ARRAY, static_cast<GLint>(l), GL_RGBA8, static_cast<GLsizei>(b.w >> l), static_cast<GLsizei>(b.h >> l), 1, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, levels[l].data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(n - 1));
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
}
// orig: (host) a new bucket for a texture of the frame's build: its index (see NewBucket)
int add_bucket(Level& L, uint32_t w, uint32_t h, std::vector<std::vector<uint32_t>>&& levels) {
    const int index = static_cast<int>(L.buckets.size() + g_new_buckets.size());
    if (g_tex_defer) {
        g_new_buckets.push_back(NewBucket{w, h, std::move(levels)});
        return index;
    }
    Bucket b;
    b.w = w;
    b.h = h;
    b.levels = static_cast<uint32_t>(levels.size());
    create_bucket_image(b, levels);
    L.buckets.push_back(b);
    return index;
}
// orig: (host) the buckets the worker's build asked for, made on the host thread (in their reserved order)
void create_new_buckets(Level& L) {
    for (const NewBucket& nb : g_new_buckets) {
        Bucket b;
        b.w = nb.w;
        b.h = nb.h;
        b.levels = static_cast<uint32_t>(nb.levels.size());
        create_bucket_image(b, nb.levels);
        L.buckets.push_back(b);
    }
    g_new_buckets.clear();
}

// orig: (host) the sky's texture whose TEX0 the frame's sky packets set: the sky asset's entry caching that TEX0 this
// frame (the per-frame cache at its +0, 0x28b680 / 0x28c3c0 / 0x28c07c), its texels (+8: offset / 16 in the
// asset's texture data *(asset + 0x14)) and palette (+0xa) as the sky's texture uploads send them (texture_uploads 0x24d760: the
// texels as PSMT8 rows, the palette 16 x 16 PSMCT32, CSM1), decoded once per level to RGBA (the GS's 0..0x80 alpha kept)
const TexRef* sky_texture(Level& L, const SceneFrame& S, uint64_t tex0) {
    uint32_t e = 0;
    for (const auto& [t, a] : S.sky_tex)
        if (t == tex0) {
            e = a;
            break;
        }
    if (e == 0 || S.sky_asset == 0) return nullptr;
    const uint32_t data = lwu(S.sky_asset + 0x14);
    const uint32_t img = data + (static_cast<uint32_t>(lhu(e + 8)) << 4), pal = data + (static_cast<uint32_t>(lhu(e + 0xa)) << 4);
    const uint64_t fmt = tex0 & ((0x3full << 20) | (0xfull << 26) | (0xfull << 30) | (0xfull << 51) | (1ull << 55));
    const auto key = std::make_tuple(img, pal, fmt);
    const auto it = L.sky_tex.find(key);
    if (it != L.sky_tex.end()) return it->second.bucket >= 0 ? &it->second : nullptr;
    TexRef& r = L.sky_tex[key];
    const uint32_t psm = fld(tex0, 20, 6), tw = fld(tex0, 26, 4), th = fld(tex0, 30, 4), cpsm = fld(tex0, 51, 4), csm = fld(tex0, 55, 1);
    const uint32_t w = 1u << tw, h = 1u << th;
    if ((psm != 0x13 && psm != 0x14) || cpsm != 0 || w > 1024 || h > 1024) {
        std::fprintf(stderr, "[mrpw] sky texture TEX0 %016llx: format not decoded (psm %x cpsm %x)\n", static_cast<unsigned long long>(tex0), psm, cpsm);
        return nullptr;
    }
    // (reads only: never a journal's page, the build may run on the scene's worker while the game's thread re-runs a pass, mrp_lod.h)
    const uint8_t* pix = static_cast<const uint8_t*>(ee_ram_ptr_read(img));
    const uint8_t* clut = static_cast<const uint8_t*>(ee_ram_ptr_read(pal));
    const uint32_t bytes = psm == 0x13 ? w * h : w * h / 2;
    if (pix == nullptr || clut == nullptr || ee_ram_ptr_read(img + bytes - 1) == nullptr) return nullptr;
    auto csm1 = [](uint32_t i) { return ((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i; };
    std::vector<uint32_t> px(static_cast<size_t>(w) * h);
    for (uint32_t i = 0; i < w * h; ++i) {
        const uint32_t idx = psm == 0x13 ? pix[i] : (pix[i >> 1] >> (4 * (i & 1))) & 0xf;
        const uint32_t ci = psm == 0x13 && csm == 0 ? csm1(idx) : idx;
        std::memcpy(&px[i], clut + 4 * ci, 4);
    }
    std::vector<std::vector<uint32_t>> lv;
    lv.push_back(std::move(px));
    r.bucket = add_bucket(L, w, h, std::move(lv));
    r.layer = 0;
    r.mips = 1;
    ++g_st.sky_textures;
    return &r;
}

// orig: (host) a world GIF stream's texture (the shrub far variants' billboards): TEX0 names it in GS
// memory, which holds it since the level load (gs_ram_upload 0x240498: the core's GS RAM block, the shrub classes' billboard images
// and palettes; shrub_class_register 0x240e80 writes the variant record's TEX0 / TEX1 / MIPTBP1 from the class entry), so it is
// decoded from the GS replica's local memory (PSMT8 + PSMCT32 CLUT, CSM1, as Gs::texel reads it; the GS's 0..0x80 alpha kept),
// the levels below 0 from MIPTBP1 (TEX1 MXL of them, 3 at most), once per level. (The frame's own uploads start past that block,
// 0x15ee78: the replica's chain, which may be running, does not write it.)
const TexRef* vram_texture(Level& L, uint64_t tex0, uint64_t tex1, uint64_t mip1) {
    const uint32_t mxl = std::min(fld(tex1, 2, 3), 3u);
    const uint64_t fmt = tex0 & ~((1ull << 34) | (3ull << 35) | (7ull << 61));
    const auto key = std::make_tuple(fmt, mxl != 0 ? mip1 : 0ull, mxl);
    const auto it = L.vram_tex.find(key);
    if (it != L.vram_tex.end()) return it->second.bucket >= 0 ? &it->second : nullptr;
    TexRef& r = L.vram_tex[key];
    const uint32_t psm = fld(tex0, 20, 6), tw = fld(tex0, 26, 4), th = fld(tex0, 30, 4), cpsm = fld(tex0, 51, 4), csa = fld(tex0, 56, 5);
    const uint32_t cbp = fld(tex0, 37, 14);
    const uint32_t w = 1u << tw, h = 1u << th;
    if (psm != 0x13 || cpsm != 0 || csa != 0 || w > 1024 || h > 1024) {
        std::fprintf(stderr, "[mrpw] world GIF texture TEX0 %016llx: format not decoded (psm %x cpsm %x csa %x)\n", static_cast<unsigned long long>(tex0), psm, cpsm, csa);
        return nullptr;
    }
    const uint8_t* vram = gs::local_memory().vram();
    uint32_t clut[256];
    for (uint32_t i = 0; i < 256; ++i) {
        const uint32_t j = (i & 0xe7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);  // (CSM1)
        std::memcpy(&clut[i], vram + gs::LocalMemory::addr32(cbp, 1, j & 0xf, j >> 4), 4);
    }
    uint32_t levels = 1;
    while (levels <= mxl && (w >> levels) >= 1 && (h >> levels) >= 1) ++levels;
    std::vector<std::vector<uint32_t>> lv(levels);
    for (uint32_t l = 0; l < levels; ++l) {
        const uint32_t tbp = l == 0 ? fld(tex0, 0, 14) : fld(mip1, 20 * (l - 1), 14);
        const uint32_t tbw = l == 0 ? fld(tex0, 14, 6) : fld(mip1, 20 * (l - 1) + 14, 6);
        const uint32_t lw = w >> l, lh = h >> l;
        std::vector<uint32_t>& px = lv[l];
        px.resize(static_cast<size_t>(lw) * lh);
        for (uint32_t y = 0; y < lh; ++y)
            for (uint32_t x = 0; x < lw; ++x) px[y * lw + x] = clut[vram[gs::LocalMemory::addr8(tbp, tbw, x, y)]];
    }
    r.bucket = add_bucket(L, w, h, std::move(lv));
    r.layer = 0;
    r.mips = levels;
    ++g_st.gif_textures;
    return &r;
}

// ---- the emissive surfaces (mrp_lighting.h EmissiveSurface; our data) ----------------------------------------------------------
// per level texture (tfrag, tie, shrub tables) its shader slot (0 none), rebuilt on the scene's worker when the list, the preview or
// the level changes; the slot goes into the texture word's bits 25..29 (key_now) while the modern lighting is on, the original's draws
// untouched
}  // namespace
void mrp_world_texture_survey();
namespace {
struct EmissiveTable {
    uint32_t gen = 0;
    int level = -1;
    bool on = false;  // (this frame: the modern lighting and the option on, a surface marked)
    std::vector<uint8_t> slot[3];
} g_emis;

// orig: (host) a class's texture (EmissiveSurface::tex) as the level's texture (-1 none), the class's count
int class_texture(const Level& L, int kind, int o_class, int tex, int* ntex) {
    if (ntex != nullptr) *ntex = 0;
    if (!L.loaded || L.world == nullptr) return -1;
    if (kind == 0) {
        if (ntex != nullptr) *ntex = static_cast<int>(L.tex[0].size());
        return tex >= 0 && static_cast<size_t>(tex) < L.tex[0].size() ? tex : -1;
    }
    if (kind == 1)
        for (const TieClass& c : L.world->ties)
            if (c.o_class == o_class) {
                if (ntex != nullptr) *ntex = static_cast<int>(c.textures.size());
                return tex >= 0 && static_cast<size_t>(tex) < c.textures.size() ? c.textures[static_cast<size_t>(tex)].texture : -1;
            }
    if (kind == 2)
        for (const ShrubClass& c : L.world->shrubs)
            if (c.o_class == o_class) {
                int found = -1, n = 0;
                for (const ShrubPacket& p : c.packets)
                    for (const TexturePrim& t : p.textures) {
                        n = std::max(n, t.local_texture + 1);
                        if (t.local_texture == tex && t.texture >= 0) found = t.texture;
                    }
                if (ntex != nullptr) *ntex = n;
                return found;
            }
    return -1;
}

// orig: (host) the table brought up to date (the frame's build, on the scene's worker)
void emissive_refresh() {
    const Level& L = g_level;
    std::vector<EmissiveSurface> list;
    EmissiveSurface pv;
    bool has_pv = false;
    const uint32_t gen = mrp_emissive_snapshot(list, &pv, &has_pv);
    const int level = L.loaded ? L.id : -1;
    if (gen != g_emis.gen || level != g_emis.level) {
        static const bool survey = std::getenv("RAC1_LIGHTS_TRACE") != nullptr;
        if (survey && level >= 0 && level != g_emis.level) mrp_world_texture_survey();  // (testing: the materials' survey)
        g_emis.gen = gen;
        g_emis.level = level;
        bool any = false;
        for (int k = 0; k < 3; ++k) g_emis.slot[k].assign(L.tex[k].size(), 0);
        auto mark = [&](const EmissiveSurface& e, uint8_t slot) {
            if (e.kind < 0 || e.kind > 2) return;
            int n = 0;
            const int t = class_texture(L, e.kind, e.o_class, e.tex, &n);
            static const bool trace = std::getenv("RAC1_LIGHTS_TRACE") != nullptr;
            if (trace && level >= 0)
                std::printf("[lights] emissive %s %d tex %d (of %d): level texture %d, slot %u\n", mrp_emissive_kind_name(e.kind), e.o_class, e.tex, n, t, slot);
            if (t < 0 || static_cast<size_t>(t) >= g_emis.slot[e.kind].size()) return;
            g_emis.slot[e.kind][static_cast<size_t>(t)] = slot;
            any = true;
        };
        for (size_t i = 0; i < list.size() && i < static_cast<size_t>(kEmissiveSlots); ++i) mark(list[i], static_cast<uint8_t>(i + 1));
        if (has_pv) mark(pv, static_cast<uint8_t>(kEmissivePreviewSlot));
        g_emis.on = any;
    }
}
bool g_emis_frame = false;  // (this frame's batches carry the slots)

// orig: (host) the GS's state along the world's packets (the registers as the replica keeps them, gs_emu.cpp) and the vertex
// queue; the triangles the GS would draw become GPU triangles with the state of their closing vertex
struct GsFront {
    std::array<uint64_t, 128> regs{};
    uint32_t rgba = 0;
    float stq[3] = {0, 0, 1};
    struct QV {
        uint32_t index;
        bool flagged;
    };
    std::vector<QV> queue;
    Frame& F;
    const SceneFrame& S;
    explicit GsFront(Frame& f) : F(f), S(f.scene) {}

    uint64_t prim() const { return fld(regs[0x1a], 0, 1) != 0 ? regs[0x00] : ((regs[0x1b] & ~7ull) | (regs[0x00] & 7)); }
    void write(uint32_t reg, uint64_t v) {
        reg &= 0x7f;
        regs[reg] = v;
        if (reg == 0x00) {
            queue.clear();
            sq.clear();  // (the GS's one vertex queue)
        } else if (reg == 0x01) rgba = static_cast<uint32_t>(v);
        else if (reg == 0x02) {
            const uint32_t s = static_cast<uint32_t>(v), t = static_cast<uint32_t>(v >> 32);
            std::memcpy(&stq[0], &s, 4);
            std::memcpy(&stq[1], &t, 4);
            stq[2] = 1.0f;
        }
    }
    Key key_now(int table) {
        const uint64_t p = prim();
        const int ctx = static_cast<int>(fld(p, 9, 1));
        Key k;
        k.prim = p;
        k.test = regs[0x47 + ctx];
        k.alpha = regs[0x42 + ctx];
        k.zbuf = regs[0x4e + ctx];
        k.fogcol = static_cast<uint32_t>(regs[0x3d] & 0xffffff);
        if (fld(p, 4, 1) == 0) return k;  // (no texture)
        const uint64_t tex0 = regs[0x06 + ctx], tex1 = regs[0x14 + ctx], clamp = regs[0x08 + ctx];
        const uint32_t tfx = fld(tex0, 35, 2), tcc = fld(tex0, 34, 1);
        const uint32_t mxl = std::min(fld(tex1, 2, 3), 3u), lcm = fld(tex1, 0, 1), l = fld(tex1, 19, 2), kk = fld(tex1, 32, 12);
        k.word = (tfx << 1) | (tcc << 3) | (mxl << 4) | (lcm << 7) | (l << 8) | (kk << 10);
        k.sampler = (fld(tex1, 5, 1) != 0 ? 1u : 0u) | (fld(tex1, 6, 3) >= 4 ? 2u : 0u) | (fld(clamp, 0, 2) == 0 ? 4u : 0u) | (fld(clamp, 2, 2) == 0 ? 8u : 0u);
        if (gif_now) {  // a world GIF stream's: by its TEX0 in GS memory
            const TexRef* r = vram_texture(g_level, tex0, tex1, regs[0x34 + ctx]);
            if (r == nullptr) {
                ++g_st.gif_tex_missing;
                k.word |= 1;
                return k;
            }
            k.bucket = r->bucket;
            k.layer = r->layer;
            k.word |= (std::min(std::min(r->mips, mxl + 1), 4u) - 1) << 22;  // (the GS samples levels 0..MXL only, as the mobys')
            ++g_st.tex_ok;
            return k;
        }
        if (table == 3) {  // the sky's: by the TEX0 its packets set
            const TexRef* r = sky_texture(g_level, S, tex0);
            if (r == nullptr) {
                ++g_st.sky_tex_missing;
                k.word |= 1;
                return k;
            }
            k.bucket = r->bucket;
            k.layer = r->layer;
            ++g_st.tex_ok;
            return k;
        }
        // the level texture: CLAMP_1's MINV field holds its index in the pass's table (the level load)
        const uint64_t ti = clamp >> 24;
        const Level& L = g_level;
        if (g_env_debug) {
            static int shown = 0;
            if (shown++ < 4)
                std::printf("[mrpw-dbg] prim %llx (PRIM %llx PRMODECONT %llx PRMODE %llx) test %llx alpha %llx zbuf %llx\n", static_cast<unsigned long long>(p),
                            static_cast<unsigned long long>(regs[0]), static_cast<unsigned long long>(regs[0x1a]), static_cast<unsigned long long>(regs[0x1b]),
                            static_cast<unsigned long long>(k.test), static_cast<unsigned long long>(k.alpha), static_cast<unsigned long long>(k.zbuf));
            static std::map<std::tuple<int, uint64_t, uint64_t>, int> seen;
            if (seen.size() < 60 && seen.insert({{table, clamp, tex0}, 1}).second)
                std::printf("[mrpw-dbg] table %d clamp %016llx tex0 %016llx tex1 %016llx -> texture %llu\n", table, static_cast<unsigned long long>(clamp), static_cast<unsigned long long>(tex0),
                            static_cast<unsigned long long>(tex1), static_cast<unsigned long long>(ti));
        }
        if (table < 0 || table > 2 || ti >= L.ref[table].size() || L.ref[table][ti].bucket < 0) {
            ++g_st.tex_missing;
            k.word |= 1;
            return k;
        }
        const TexRef& r = L.ref[table][ti];
        k.bucket = r.bucket;
        k.layer = r.layer;
        k.word |= (std::min(std::min(r.mips, mxl + 1), 4u) - 1) << 22;  // (the GS samples levels 0..MXL only, as the mobys')
        if (g_emis_frame && ti < g_emis.slot[table].size()) k.word |= static_cast<uint32_t>(g_emis.slot[table][ti]) << 25;  // (an emissive surface's slot)
        ++g_st.tex_ok;
        return k;
    }
    void triangle(uint32_t a, uint32_t b, uint32_t c, int table) {
        Key k = key_now(table);
        F.v[c].word = k.word;
        F.v[c].layer = k.layer;
        F.v[c].state[0] = draw_flags(k, 0);
        F.v[c].state[1] = k.fogcol;
        k.word = 0;
        k.layer = 0;
        const RKey rk = batch_key(k, false, sky_now);
        if (F.batches.empty() || !(F.batches.back().rk == rk) || (rk.pass2 != 0 && !F.merge_afail && !(F.batches.back().key == k))) {  // (an AFAIL second pass: as one GS state, see draw_frame)
            F.batches.push_back(Batch{k, false, sky_now, static_cast<uint32_t>(F.idx.size()), 0});
            F.batches.back().rk = rk;
        }
        F.idx.insert(F.idx.end(), {a, b, c});
        if (fld(k.test, 0, 1) != 0 && fld(k.test, 1, 3) != 1) F.batches.back().ate = true;  // (ATST ALWAYS: no test)
        F.batches.back().count += 3;
        ++g_st.triangles;
    }

    // ---- the sky: the GS register writes of its streams (SceneFrame::sky), its vertices as the GS kicks them; drawn on
    // the GS's frame (projection 1: (X, Y, Z) as kicked, w = 1: the GS's affine S / T / Q interpolation) or in the world's projection
    // (0: the direction the frame's projection A sees the point (X, Y) in, as a point at infinity: the sky behind everything)
    struct SkyRaw {
        uint32_t index;
        float x, y, z, s, t, q, fog;
        uint32_t rgba;
    };
    std::vector<SkyRaw> sq;  // the sky's vertex queue (the GS's)
    float qreg = 1.0f;       // the GS's Q (RGBAQ's, PACKED STQ's)
    uint32_t fogreg = 0;
    bool sky_now = false;
    int sky_mode = 0;
    glm::dmat4 sky_vp{1.0}, sky_a{1.0};
    // orig: (host) the direction (world axes) of the points the frame's projection A (A p: the GS's homogeneous screen point of
    // p = (world - camera) x 1024) puts at the GS's (X, Y): (A0 - X A3) . d = 0, (A1 - Y A3) . d = 0, in front: A3 . d > 0
    glm::dvec3 sky_dir(double X, double Y) const {
        const glm::dvec3 r0(sky_a[0][0], sky_a[1][0], sky_a[2][0]), r1(sky_a[0][1], sky_a[1][1], sky_a[2][1]), r3(sky_a[0][3], sky_a[1][3], sky_a[2][3]);
        glm::dvec3 d = glm::cross(r0 - X * r3, r1 - Y * r3);
        if (glm::dot(r3, d) < 0.0) d = -d;
        const double n = glm::length(d);
        return n > 0.0 ? d / n : d;
    }
    // a world pass's GIF stream (gif_now): its points (X, Y, Z) taken back to the world through the pass's view
    // (hp = (X, Y, Z, 1) up to its w: the EE's quads are projected as the pass's programs are, by A), drawn with the world
    bool gif_now = false;
    int stream_table = 3;
    glm::dmat4 gif_view{1.0};
    uint32_t sky_gpuv(float x, float y, float z, float s, float t, float q, uint32_t rgba_, float fog) {
        GpuV v{};
        v.vu[0] = x;
        v.vu[1] = y;
        v.vu[2] = z;
        v.vu[3] = 1.0f;
        glm::dvec4 wp;
        if (gif_now) {
            wp = gif_view * glm::dvec4(x, y, z, 1.0);
            wp = std::fabs(wp.w) > 1e-30 ? wp / wp.w : glm::dvec4(0.0);
        } else {
            wp = glm::dvec4(sky_dir(x, y), 0.0);
        }
        for (int l = 0; l < 4; ++l) v.wp[l] = static_cast<float>(wp[l]);
        double w = 1.0;  // (STQ x the clip w: the GPU's perspective interpolation then gives the GS's affine S / Q)
        if (sky_mode == 0) {
            w = (sky_vp * wp).w;
            if (!(w > 1e-9)) w = 1e-9;
        }
        v.stq[0] = static_cast<float>(s * w);
        v.stq[1] = static_cast<float>(t * w);
        v.stq[2] = static_cast<float>(q * w);
        v.lodq = q;
        std::memcpy(v.rgba, &rgba_, 4);
        v.fog = fog;
        const uint32_t index = static_cast<uint32_t>(F.v.size());
        F.v.push_back(v);
        ++(gif_now ? g_st.gif_vertices : g_st.sky_vertices);
        return index;
    }
    // orig: (host) an XYZ(F)2 / 3 write: the vertex, the GS's primitive assembly (gs_emu.cpp vertex_kick: triangles, strips, fans,
    // sprites as two triangles; points and lines counted, not drawn)
    void sky_vertex(uint64_t v, bool xyzf, bool draw) {
        float st[2];
        std::memcpy(st, stq, 8);
        SkyRaw r{0, static_cast<float>(v & 0xffff) / 16.0f, static_cast<float>((v >> 16) & 0xffff) / 16.0f,
                 xyzf ? static_cast<float>((v >> 32) & 0xffffff) : static_cast<float>(static_cast<uint32_t>(v >> 32)), st[0], st[1], qreg,
                 static_cast<float>(xyzf ? (v >> 56) & 0xff : fogreg), rgba};
        r.index = sky_gpuv(r.x, r.y, r.z, r.s, r.t, r.q, r.rgba, r.fog);
        sq.push_back(r);
        const int type = static_cast<int>(fld(regs[0x00], 0, 3));
        const size_t need = type == 0 ? 1 : type <= 2 || type == 6 ? 2 : type <= 5 ? 3 : 0;
        if (need == 0) {
            sq.clear();
            return;
        }
        if (sq.size() < need) return;
        if (draw) {
            if (type >= 3 && type <= 5) {
                const SkyRaw& a = type == 5 ? sq.front() : sq[sq.size() - 3];
                triangle(a.index, sq[sq.size() - 2].index, sq.back().index, stream_table);
                ++(gif_now ? g_st.gif_triangles : g_st.sky_triangles);
            } else if (type == 6) {  // a sprite: the corners (S, T from the two vertices'), the second vertex's colour, Z, F
                const SkyRaw a = sq[sq.size() - 2], b = sq.back();
                const uint32_t c01 = sky_gpuv(b.x, a.y, b.z, b.s, a.t, b.q, b.rgba, b.fog), c10 = sky_gpuv(a.x, b.y, b.z, a.s, b.t, a.q, b.rgba, b.fog);
                const uint32_t c00 = sky_gpuv(a.x, a.y, b.z, a.s, a.t, a.q, b.rgba, b.fog);
                triangle(c00, c01, c10, stream_table);
                triangle(c01, b.index, c10, stream_table);
                ++g_st.sky_sprites;
            } else {
                ++g_st.sky_other;
            }
        }
        if (type == 2) sq.erase(sq.begin(), sq.end() - 1);
        else if (type == 4) sq.erase(sq.begin());
        else if (type == 5) sq.erase(sq.begin() + 1);
        else sq.clear();
    }
    // orig: (host) a sky packet (or, a world pass's GIF packet: gif_packet): its register writes in order (the GS's:
    // gs_emu.cpp write_reg / packed)
    void gif_packet(const ScenePacket& p) {
        if (p.view < 0 || static_cast<size_t>(p.view) >= S.views.size()) return;
        std::memcpy(&gif_view[0][0], S.views[static_cast<size_t>(p.view)].data(), sizeof(double) * 16);
        gif_now = true;
        stream_table = p.table;
        stream_writes(p);
        gif_now = false;
        stream_table = 3;
    }
    void sky_packet(const ScenePacket& p) {
        sky_now = true;
        stream_writes(p);
        sky_now = false;
    }
    void stream_writes(const ScenePacket& p) {
        static const bool dump = std::getenv("RAC1_MRPW_SKY_DUMP") != nullptr;  // (testing: the sky's register writes but the vertices', once a run)
        static int dumped = 0;
        for (uint32_t k = p.sky_first; k < p.sky_first + p.sky_count && k < S.sky.size(); ++k) {
            const SkyWrite& w = S.sky[k];
            if (dump && dumped < 400 && w.mode == 0 && w.reg != 0x01 && w.reg != 0x02 && w.reg != 0x04 && w.reg != 0x05) {
                ++dumped;
                std::printf("[mrpw-sky] packet %u reg %02x %016llx\n", p.sky_first, w.reg, static_cast<unsigned long long>(w.v));
            }
            switch (w.mode) {
            case 1: rgba = static_cast<uint32_t>(w.v); break;
            case 2: {
                const uint32_t s = static_cast<uint32_t>(w.v), t = static_cast<uint32_t>(w.v >> 32);
                std::memcpy(&stq[0], &s, 4);
                std::memcpy(&stq[1], &t, 4);
                std::memcpy(&qreg, &w.q, 4);
                break;
            }
            default:
                switch (w.reg & 0x7f) {
                case 0x00: write(0x00, w.v); sq.clear(); break;
                case 0x01: {
                    rgba = static_cast<uint32_t>(w.v);
                    const uint32_t q = static_cast<uint32_t>(w.v >> 32);
                    std::memcpy(&qreg, &q, 4);
                    regs[0x01] = w.v;
                    break;
                }
                case 0x02: {
                    const uint32_t s = static_cast<uint32_t>(w.v), t = static_cast<uint32_t>(w.v >> 32);
                    std::memcpy(&stq[0], &s, 4);
                    std::memcpy(&stq[1], &t, 4);
                    regs[0x02] = w.v;
                    break;
                }
                case 0x04: case 0x0c: sky_vertex(w.v, true, (w.reg & 0x7f) == 0x04); break;
                case 0x05: case 0x0d: sky_vertex(w.v, false, (w.reg & 0x7f) == 0x05); break;
                case 0x0a: fogreg = static_cast<uint32_t>(w.v >> 56) & 0xff; regs[0x0a] = w.v; break;
                default: write(w.reg, w.v); break;
                }
            }
        }
    }
    // a vertex kick (XYZF2 / XYZ2 PACKED): the GPU vertex, the GS's primitive assembly (gs_emu.cpp vertex_kick)
    void vertex(const ScenePacket& p, size_t qi, const std::array<uint32_t, 4>& q, bool xyzf) {
        GpuV v{};
        const std::array<float, 4> hp = qi < p.k.hp.size() ? p.k.hp[qi] : std::array<float, 4>{};
        const float W = hp[3];
        if (p.view >= 0 && static_cast<size_t>(p.view) < S.views.size()) {
            const std::array<double, 16>& m = S.views[static_cast<size_t>(p.view)];
            for (int r = 0; r < 4; ++r)
                v.wp[r] = static_cast<float>(m[r] * hp[0] + m[4 + r] * hp[1] + m[8 + r] * hp[2] + m[12 + r] * hp[3]);
        }
        const float X = static_cast<float>(q[0] & 0xffff) / 16.0f, Y = static_cast<float>(q[1] & 0xffff) / 16.0f;
        const float Z = xyzf ? static_cast<float>((q[2] >> 4) & 0xffffff) : static_cast<float>(q[2]);
        if (g_env_debug) {
            static int shown[6] = {};
            if (shown[static_cast<int>(p.prog)]++ < 3)
                std::printf("[mrpw-dbg] prog %d vertex hp %g %g %g %g stq %g %g %g xyz %08x %08x %08x %08x world %g %g %g %g\n", static_cast<int>(p.prog), static_cast<double>(hp[0]),
                            static_cast<double>(hp[1]), static_cast<double>(hp[2]), static_cast<double>(hp[3]), static_cast<double>(stq[0]), static_cast<double>(stq[1]),
                            static_cast<double>(stq[2]), q[0], q[1], q[2], q[3], static_cast<double>(v.wp[0]), static_cast<double>(v.wp[1]), static_cast<double>(v.wp[2]),
                            static_cast<double>(v.wp[3]));
        }
        const bool flagged = qi < p.k.clip.size() && p.k.clip[qi] != 0;
        const bool clipping = p.prog == Program::TfragB || p.prog == Program::TieB || p.prog == Program::ShrubB;
        // the program's screen formula X = (hp.x k scale.x + offset.x hp.w) / hp.w (k: 1, or the vertex's Q x hp.w: tfrag C0.x, shrub B h,
        // tie B q); in front of the camera where Q > 0, i.e. hp.w k > 0: the homogeneous point taken with the sign that makes its w
        // positive (the tfrag view's w is negative, C0.x too)
        const float k = p.scr_k != 0 ? stq[2] * W : 1.0f;
        const float sgn = k < 0.0f ? -1.0f : 1.0f;
        const float Wg = W * sgn;
        if (Wg > 0.0f && !(clipping && flagged)) {  // (the GS's coordinates as kicked: the 1/16 px snap)
            v.vu[0] = X * Wg;
            v.vu[1] = Y * Wg;
            v.vu[2] = Z * Wg;
            v.vu[3] = Wg;
        } else {  // a clipping program's vertex outside its volume (its 16-bit coordinates wrap; behind the camera): the linear form of
                  // the program's screen formula, which the GPU clips as the program does
            for (int l = 0; l < 3; ++l) v.vu[l] = sgn * (hp[static_cast<size_t>(l)] * k * p.scr_scale[l] + p.scr_off[l] * W);
            v.vu[3] = Wg;
        }
        // (STQ undivided: x the positive w, the GPU's perspective interpolation then gives the GS's S / Q)
        const float ws = Wg != 0.0f ? Wg : 1.0f;
        v.stq[0] = stq[0] * ws;
        v.stq[1] = stq[1] * ws;
        v.stq[2] = stq[2] * ws;
        v.lodq = stq[2];
        std::memcpy(v.rgba, &rgba, 4);
        v.fog = xyzf ? static_cast<float>((q[3] >> 4) & 0xff) : 0.0f;
        const uint32_t index = static_cast<uint32_t>(F.v.size());
        F.v.push_back(v);
        ++g_st.vertices;
        const bool adc = ((q[3] >> 15) & 1) != 0;
        queue.push_back({index, flagged});
        const int type = static_cast<int>(fld(regs[0x00], 0, 3));
        if (type != 3 && type != 4 && type != 5) {
            queue.clear();
            return;
        }
        if (queue.size() < 3) return;
        const QV& a = type == 5 ? queue.front() : queue[queue.size() - 3];
        const QV& b = queue[queue.size() - 2];
        const QV& c = queue.back();
        if (adc) ++g_st.adc;
        else if (p.prog == Program::TieB && (a.flagged || b.flagged || c.flagged) && !(F.in.projection == 0 && F.in.widen > 1.0f))
            ++g_st.clip_dropped;  // (tie B drops them; in a widened view the flags are the 4:3 frame's: drawn whole, the GPU clips)
        else triangle(a.index, b.index, c.index, p.table);
        if (type == 3) queue.clear();
        else if (type == 4) queue.erase(queue.begin());
        else queue.erase(queue.begin() + 1);
    }
    void packet(const ScenePacket& p) {
        const auto& pk = p.k.q;
        size_t i = 0;
        while (i < pk.size()) {
            const uint64_t lo = static_cast<uint64_t>(pk[i][0]) | static_cast<uint64_t>(pk[i][1]) << 32;
            const uint64_t hi = static_cast<uint64_t>(pk[i][2]) | static_cast<uint64_t>(pk[i][3]) << 32;
            ++i;
            const uint32_t nloop = lo & 0x7fff, flg = (lo >> 58) & 3, nreg = ((lo >> 60) & 0xf) == 0 ? 16 : (lo >> 60) & 0xf;
            if (flg != 0) break;  // (the world programs kick PACKED only)
            if (((lo >> 46) & 1) != 0) write(0x00, (lo >> 47) & 0x7ff);
            for (uint32_t n = 0; n < nloop && i < pk.size(); ++n)
                for (uint32_t r = 0; r < nreg && i < pk.size(); ++r, ++i) {
                    const uint32_t d = static_cast<uint32_t>(hi >> (4 * r)) & 0xf;
                    const auto& q = pk[i];
                    switch (d) {
                    case 0x0: write(0x00, q[0] & 0x7ff); break;
                    case 0x1: rgba = (q[0] & 0xff) | ((q[1] & 0xff) << 8) | ((q[2] & 0xff) << 16) | ((q[3] & 0xff) << 24); break;
                    case 0x2: std::memcpy(stq, q.data(), 12); break;
                    case 0x4: case 0xc: vertex(p, i, q, true); break;
                    case 0x5: case 0xd: vertex(p, i, q, false); break;
                    case 0xe: write(q[2] & 0xff, static_cast<uint64_t>(q[0]) | static_cast<uint64_t>(q[1]) << 32); break;
                    default: break;
                    }
                }
            if (((lo >> 15) & 1) != 0 && i >= pk.size()) break;
        }
    }
};

bool g_shadow_emit = false;  // gpu_packet's batches are the casters'
uint16_t g_shadow_emit_views = 0xffff;  // and their shadow views

// orig: (host) a GPU path packet (tie A / shrub A): its instance record, its template's runs as draw commands with the GS state
// at each (its texture prims' A+D from the instance's VU1 memory applied to the registers as the GS would)
void gpu_packet(GsFront& g, Frame& F, const ScenePacket& p, const glm::dmat4& vp) {
    const Level& L = g_level;
    const void* key = p.tie != nullptr ? static_cast<const void*>(p.tie) : static_cast<const void*>(p.shrub);
    const auto it = L.tpl.find(key);
    if (it == L.tpl.end() || p.inst >= F.scene.insts.size()) return;
    const Tpl& t = it->second;
    const SceneInst& si = F.scene.insts[p.inst];
    // (an instance drawn by several packets - the host's ties / shrubs, each packet of its LOD - made once)
    if (F.inst_of.size() < F.scene.insts.size()) F.inst_of.resize(F.scene.insts.size(), 0xffffffffu);
    uint32_t inst = F.inst_of[p.inst];
    if (inst == 0xffffffffu) {
    InstGpu ig{};
    glm::dmat4 R;
    for (int r = 0; r < 4; ++r) {
        R[r] = glm::dvec4(si.rows[r][0], si.rows[r][1], si.rows[r][2], si.rows[r][3]);
        for (int l = 0; l < 4; ++l) ig.rows[r * 4 + l] = si.rows[r][l];
    }
    glm::dmat4 V(0.0);
    if (p.view >= 0 && static_cast<size_t>(p.view) < F.scene.views.size()) std::memcpy(&V[0][0], F.scene.views[static_cast<size_t>(p.view)].data(), sizeof(double) * 16);
    const glm::mat4 clip = glm::mat4(vp * V * R);
    std::memcpy(ig.clip, glm::value_ptr(clip), sizeof ig.clip);
    std::memcpy(ig.morph, si.morph, sizeof ig.morph);
    ig.info[0] = si.colour_first;
    ig.info[1] = si.fog;
    ig.info[2] = si.alpha;
    ig.info[3] = (p.shrub != nullptr ? 1u : 0u) | (si.resident ? 2u : 0u);
    inst = static_cast<uint32_t>(F.insts.size());
    F.inst_of[p.inst] = inst;
    F.insts.push_back(ig);
    }
    const uint32_t n = p.tie != nullptr ? 5u : 4u;  // (the A+D qwords of a tie texture slot / a shrub prim)
    auto apply = [&](int slot) {
        for (uint32_t r = 0; r < n; ++r) {
            const uint32_t at = static_cast<uint32_t>(slot) * n + r;
            if (at >= p.ntex) break;
            const auto& q = F.scene.tex[p.tex + at];
            g.write(q[2] & 0xff, static_cast<uint64_t>(q[0]) | static_cast<uint64_t>(q[1]) << 32);
        }
    };
    int applied = -1;
    for (const TplRun& run : t.runs) {
        if (run.slot >= 0 && run.slot != applied) {
            apply(run.slot);
            applied = run.slot;
        }
        g.regs[0] = run.prim;
        Key k = g.key_now(p.table);
        const uint32_t cmd = static_cast<uint32_t>(F.cmds.size() / 8);
        // (the texture's unit in the frame's current epoch; a new epoch, so a new batch, when the 15 units are taken)
        uint32_t unit = 0;
        if (k.bucket >= 0 && fld(k.prim, 4, 1) != 0) {
            if (F.epochs.empty()) F.epochs.emplace_back();
            auto& ep = F.epochs.back();
            const std::pair<int, uint32_t> combo{k.bucket, k.sampler};
            size_t u = std::find(ep.begin(), ep.end(), combo) - ep.begin();
            if (u == ep.size()) {
                if (ep.size() == 15) {
                    F.epochs.emplace_back();
                    u = 0;
                }
                F.epochs.back().push_back(combo);
            }
            unit = static_cast<uint32_t>(u) + 1;
        }
        F.cmds.insert(F.cmds.end(), {inst, k.word, k.layer, draw_flags(k, unit), k.fogcol, 0u, 0u, 0u});
        k.word = 0;
        k.layer = 0;
        RKey rk = batch_key(k, true, false);
        rk.shadow = g_shadow_emit;
        rk.views = g_shadow_emit ? g_shadow_emit_views : 0;
        const int epoch = F.epochs.empty() ? -1 : static_cast<int>(F.epochs.size()) - 1;
        if (F.batches.empty() || F.batches.back().tfrag || !(F.batches.back().rk == rk) || (rk.pass2 != 0 && !F.merge_afail && !(F.batches.back().key == k)) || (unit != 0 && F.batches.back().epoch >= 0 && F.batches.back().epoch != epoch)) {
            F.batches.push_back(Batch{k, true, false, static_cast<uint32_t>(F.draws.size()), 0});
            F.batches.back().rk = rk;
        }
        if (unit != 0) F.batches.back().epoch = epoch;
        F.draws.push_back(DrawCmd{run.count, 1, t.first_index + run.first, static_cast<int32_t>(t.first_vertex), cmd});
        if (fld(k.test, 0, 1) != 0 && fld(k.test, 1, 3) != 1) F.batches.back().ate = true;  // (ATST ALWAYS: no test)
        ++F.batches.back().count;
        g_st.triangles += run.count / 3;
    }
    if (t.last_slot >= 0 && t.last_slot != applied) apply(t.last_slot);
    if (t.has_prim) g.regs[0] = t.last_prim;
    g.queue.clear();
    ++g_st.gpu_packets;
}

// orig: (host) a GPU path tfrag draw (mrp_tfrag_gpu.h): its vertices / triangles laid out in the compute program's buffers;
// the GS state along its strips replayed as GsFront would from its kicks (each run's texture prim's 5 A+D qwords - the image's, as
// the level load left them in EE memory -, the strips' tag 0x292 writing PRIM), each run's Key giving its draw's texture / GS state
// and the batches (as GsFront::triangle: a new batch when the GL state changes, or for an AFAIL second pass when the GS state does)
void tfrag_packet(GsFront& g, Frame& F, const ScenePacket& p) {
    const SceneFrame& S = F.scene;
    const TfragDraw& d = S.tfrag.draws[static_cast<size_t>(p.tfrag)];
    if (d.entry >= g_tlevel.entries.size()) return;
    const TfragEntryGpu& e = g_tlevel.entries[d.entry];
    const TfragPlan* pl = e.plan(d.lod, d.code, d.b);
    if (pl == nullptr) return;
    TfragGpuFrame& T = F.tf;
    TfragGpuDraw gd{};
    gd.image = e.gpu_image[d.lod];
    gd.nq = static_cast<uint32_t>(e.image[d.lod].size() / 16);
    gd.plan = pl->gpu_first;
    gd.colour = e.colour_n != 0 ? e.colour_slot : 0xffffffffu;
    gd.col_addr = e.colour_addr[d.lod];
    gd.col_count = e.colour_count[d.lod];
    gd.view = d.view;
    gd.consts = d.consts;
    gd.wview = p.view >= 0 ? static_cast<uint32_t>(p.view) : 0;
    if (g_shadow_emit) gd.flags |= 0x80000000u;  // (a caster, no guard band ADC)
    gd.vbase = T.nverts;
    gd.ibase = T.nidx;
    gd.rbase = static_cast<uint32_t>(T.runs.size() / 4);
    T.nverts += static_cast<uint32_t>(pl->verts.size());
    T.nidx += 3 * pl->ntris;
    const auto& K = S.tfrag.consts[d.consts];
    const uint64_t tag = static_cast<uint64_t>(K[8]) | static_cast<uint64_t>(K[9]) << 32;  // (0x292)
    const uint64_t prim = (tag >> 47) & 0x7ff;
    const std::vector<uint8_t>& img = e.image[d.lod];
    for (size_t r = 0; r < pl->runs.size(); ++r) {
        const uint32_t at = pl->events[r].at;
        for (uint32_t k = 0; k < 5; ++k) {
            uint32_t q[4] = {};
            if (16u * (at + k) + 16u <= img.size()) std::memcpy(q, img.data() + 16u * (at + k), 16);
            g.write(q[2] & 0xff, static_cast<uint64_t>(q[0]) | static_cast<uint64_t>(q[1]) << 32);
        }
        g.write(0x00, prim);
        Key k = g.key_now(0);
        const uint32_t ntri = pl->runs[r].ntri;
        // the texture's unit in the frame's epoch, as the tie / shrub GPU path (gpu_packet): a batch's draws then share the GL state only
        // (the CPU path's batches break at every texture bucket / sampler change: ~60..180 tfrag batches a frame)
        uint32_t unit = 0;
        if (ntri != 0 && k.bucket >= 0 && fld(k.prim, 4, 1) != 0) {
            if (F.epochs.empty()) F.epochs.emplace_back();
            auto& ep = F.epochs.back();
            const std::pair<int, uint32_t> combo{k.bucket, k.sampler};
            size_t u = std::find(ep.begin(), ep.end(), combo) - ep.begin();
            if (u == ep.size()) {
                if (ep.size() == 15) {
                    F.epochs.emplace_back();
                    u = 0;
                }
                F.epochs.back().push_back(combo);
            }
            unit = static_cast<uint32_t>(u) + 1;
        }
        T.runs.insert(T.runs.end(), {k.word, k.layer, draw_flags(k, unit), k.fogcol});
        if (ntri == 0) continue;
        k.word = 0;
        k.layer = 0;
        RKey rk = batch_key(k, true, false);
        rk.shadow = g_shadow_emit;
        rk.views = g_shadow_emit ? g_shadow_emit_views : 0;
        const int epoch = F.epochs.empty() ? -1 : static_cast<int>(F.epochs.size()) - 1;
        if (F.batches.empty() || !F.batches.back().tfrag || !(F.batches.back().rk == rk) || (rk.pass2 != 0 && !F.merge_afail && !(F.batches.back().key == k)) ||
            (unit != 0 && F.batches.back().epoch >= 0 && F.batches.back().epoch != epoch)) {
            F.batches.push_back(Batch{k, false, false, gd.ibase + 3 * pl->runs[r].first_tri, 0});
            F.batches.back().rk = rk;
            F.batches.back().tfrag = true;
        }
        if (unit != 0) F.batches.back().epoch = epoch;
        if (fld(k.test, 0, 1) != 0 && fld(k.test, 1, 3) != 1) F.batches.back().ate = true;  // (ATST ALWAYS: no test)
        F.batches.back().count += 3 * ntri;
        g_st.triangles += ntri;
    }
    g.queue.clear();
    T.draws.push_back(gd);
    ++g_st.tfrag_gpu_draws;
}

// orig: (host), RAC1_MRPW_TFRAG_VERIFY: the CPU reference's vertices / triangles of a GPU path tfrag draw (its kicks through a
// GsFront of its own from the same GS registers), kept to be compared with the compute program's
void tfrag_expect(const GsFront& g, Frame& F, const ScenePacket& p, uint32_t vbase) {
    static Frame tmp;
    tmp.v.clear();
    tmp.idx.clear();
    tmp.batches.clear();
    tmp.scene.views = F.scene.views;
    GsFront g2(tmp);
    g2.regs = g.regs;
    g2.packet(p);
    Frame::TfragExpect x;
    x.vbase = vbase;
    x.nverts = static_cast<uint32_t>(tmp.v.size());
    x.v = tmp.v;
    x.tri = tmp.idx;
    F.tf_expect.push_back(std::move(x));
}

// orig: (host) the frame's GPU vertices / triangles / batches from the scene
void build_triangles(Frame& F) {
    const Frame::BuildInputs& in = F.in;
    emissive_refresh();
    g_emis_frame = g_emis.on && in.modern_shadows != 0;  // (RAC1_EMISSIVE=0: the shaders ignore the emissive slots, uEmis[0])
    F.v.clear();
    F.idx.clear();
    F.batches.clear();
    F.insts.clear();
    F.cmds.clear();
    F.draws.clear();
    F.epochs.clear();
    F.inst_of.assign(F.scene.insts.size(), 0xffffffffu);
    F.tf = TfragGpuFrame{};
    F.tf_expect.clear();
    for (const auto& v : F.scene.tfrag.views) F.tf.vu_views.insert(F.tf.vu_views.end(), v.begin(), v.end());
    for (const auto& c : F.scene.tfrag.consts) F.tf.consts.insert(F.tf.consts.end(), c.begin(), c.end());
    if (!F.scene.tfrag.draws.empty())
        for (const auto& v : F.scene.views) F.tf.views.insert(F.tf.views.end(), v.begin(), v.end());
    {
        static const char* env = std::getenv("RAC1_MRPW_MERGE_AFAIL");
        F.merge_afail = env != nullptr ? std::atoi(env) != 0 : (F.scene.stat_draws[0] + F.scene.stat_draws[1]) != 0;
    }
    GsFront g(F);
    // the GS's registers as the replica has them before this frame's chain (the frame packets set them again: the scene's
    // DIRECT writes from the chain's start)
    for (uint32_t r = 0; r < 128; ++r) g.regs[r] = F.gs_regs[r];  // (taken before the frame's chain: mrp_world_begin_frame_async)
    glm::mat4 vpm;
    std::memcpy(&vpm[0][0], in.vp, sizeof in.vp);
    const glm::dmat4 vp(vpm);
    size_t ri = 0;
    const auto& regs = F.scene.regs;
    static const int only = std::getenv("RAC1_MRPW_ONLY") != nullptr ? std::atoi(std::getenv("RAC1_MRPW_ONLY")) : -1;  // (testing: one program's packets)
    static const bool no_gpu = std::getenv("RAC1_MRPW_CPU") != nullptr;  // (testing: every packet by the CPU reference's kicks)
    g.sky_mode = in.projection != 0 ? 1 : 0;
    g.sky_vp = vp;
    std::memcpy(&g.sky_a[0][0], F.scene.sky_a.data(), sizeof(double) * 16);
    F.shadow_casters = false;
    bool cast_have[2] = {};
    std::array<uint64_t, 128> cast_regs[2]{};
    int32_t cast_view[2] = {-1, -1};
    for (size_t pi = 0; pi < F.scene.packets.size(); ++pi) {
        while (ri < regs.size() && regs[ri].before <= pi) {
            g.write(regs[ri].reg, regs[ri].value);
            ++ri;
        }
        const ScenePacket& p = F.scene.packets[pi];
        // with the lighting's casters made (g_shframe), the frame's shrubs - their far variants camera-facing quads, the
        // faded ones without depth writes - are the view's only: the casters' full shrub models cast instead
        // ... and so are the clipping programs' draws (tie B, tfrag B: their triangles cut to the view): the casters' whole instances cast instead
        // ... and every tfrag of the frame (the casters' own tfrags at their distance's LOD, mrp_tfrag_gpu.cpp tfrag_shadow_decide)
        static const bool keep_tfrag = std::getenv("RAC1_SHADOW_TFRAG_KEEP") != nullptr;
        g_view_only_emit = g_shframe.on[1] && !p.sky && !p.shadow &&
                           (p.table == 2 || p.prog == Program::TieB || p.prog == Program::TfragB || (!keep_tfrag && p.tfrag >= 0 && g_tfrag_shadow_on));
        if (p.sky) {  // the sky
            if (in.sky) g.sky_packet(p);
            continue;
        }
        if (p.gif) {  // a world pass's GIF vertices (the shrub far variants)
            if (p.table == 0 ? in.tfrag : p.table == 1 ? in.tie : in.shrub) g.gif_packet(p);
            continue;
        }
        const bool shown = p.table == 0 ? in.tfrag : p.table == 1 ? in.tie : in.shrub;
        if (!shown && !p.state) continue;
        if (only >= 0 && static_cast<int>(p.prog) != only) continue;
        if (p.tfrag >= 0 && p.shadow) {  // (a caster outside the view, its GS state replay not left to the next packets)
            const auto saved = g.regs;
            g_shadow_emit = true;
            g_shadow_emit_views = p.shadow_views;
            tfrag_packet(g, F, p);
            g_shadow_emit = false;
            g.regs = saved;
            g.queue.clear();
            F.shadow_casters = true;
        } else if (p.tfrag >= 0) {
            if (g_tfrag_verify && !p.k.q.empty()) tfrag_expect(g, F, p, F.tf.nverts);
            tfrag_packet(g, F, p);
        } else if ((p.tie != nullptr || p.shrub != nullptr) && !(no_gpu && !p.k.q.empty())) {
            const int kind = p.tie != nullptr ? 0 : 1;
            if (!cast_have[kind] && p.view >= 0) {  // (the GS state and view the casters' packets of that kind start from)
                cast_have[kind] = true;
                cast_regs[kind] = g.regs;
                cast_view[kind] = p.view;
            }
            gpu_packet(g, F, p, vp);
        } else g.packet(p);
        ++g_st.packets;
    }
    g_view_only_emit = false;
    // the modern lighting's ties / shrubs outside the view (g_shframe): GPU path packets as the host's ties / shrubs (emit_static),
    // their batches the shadow maps' only
    if (g_shframe.on[0] && (cast_have[0] || cast_have[1])) {
        const StaticFrame& sf = g_shframe;
        const uint32_t tex_base = static_cast<uint32_t>(F.scene.tex.size());
        F.scene.tex.insert(F.scene.tex.end(), g_slevel.tex.begin(), g_slevel.tex.end());
        const uint32_t inst_base = static_cast<uint32_t>(F.scene.insts.size());
        for (const StaticInst& si : sf.insts) {
            SceneInst x;
            std::memcpy(x.rows, si.rows, sizeof x.rows);
            std::memcpy(x.morph, si.morph, sizeof x.morph);
            x.colour_first = si.slot;
            x.resident = true;
            x.alpha = 0x80;
            F.scene.insts.push_back(x);
        }
        g_shadow_emit = true;
        for (int kind = 0; kind < 2; ++kind) {
            const int from = cast_have[kind] ? kind : 1 - kind;
            g.regs = cast_regs[from];
            g.queue.clear();
            for (const StaticDraw& d : sf.draws[kind]) {
                ScenePacket sp;
                sp.prog = kind == 0 ? Program::TieA : Program::ShrubA;
                sp.view = cast_view[from];
                sp.table = kind + 1;
                sp.tie = d.pk->tie;
                sp.shrub = d.pk->shrub;
                sp.tex = tex_base + d.pk->tex_first;
                sp.ntex = d.pk->ntex;
                sp.inst = inst_base + d.inst;
                g_shadow_emit_views = sf.insts[d.inst].mask;
                gpu_packet(g, F, sp, vp);
            }
        }
        g_shadow_emit = false;
        F.shadow_casters = true;
    }
    g_st.batches += F.batches.size();
}

// ---- drawing -----------------------------------------------------------------------------------

void set_frame_uniforms(const Loc& l, const gs::Viewport* vp, int mode) {
    glUniform1i(l.mode, mode);
    float vpm[16];
    mrp_world_view_proj(vpm);
    glUniformMatrix4fv(l.vp, 1, GL_FALSE, vpm);
    float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
    if (fb_w <= 0.0f) fb_w = 512.0f;
    if (fb_h <= 0.0f) fb_h = 416.0f;
    glUniform4f(l.frame, fb_w, fb_h, vp != nullptr ? vp->xscale_3d : 1.0f, 0.0f);
    double da = 0.0, db = 0.0;
    const bool has = render_gs_depth(da, db);
    // XYOFFSET_1 of the frame's draw environment (0x13cf10)
    const uint64_t xo = g_frame.env_xyoff;
    float x = static_cast<float>(xo & 0xffff), y = static_cast<float>((xo >> 32) & 0xffff);
    if (xo == 0) {
        x = (2048.0f - fb_w * 0.5f) * 16.0f;
        y = (2048.0f - fb_h * 0.5f) * 16.0f;
    }
    glUniform4f(l.xyoff, x, y, has ? static_cast<float>(da) : 0.5f, has ? static_cast<float>(db) : 0.0f);
    static const GLint units[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    glUniform1iv(l.tex, 16, units);

    glUniform1i(l.mip, g_mrp_world_mip);
    glUniform1i(l.shade, g_render_shade);
    mrp_lighting_bind_off(l.sh);
}

// orig: (host) the resident colours' changes since the last draw sent (a new level: the whole table), the buffer bound
void upload_resident_colours() {
    ResidentColours& rc = g_rcols;
    const size_t bytes = std::max<size_t>(rc.mirror.size() * 4, 16);
    if (vk::is_active()) {
        if (rc.all_dirty || bytes != g_rcol_size) {
            g_rcol_size = bytes;
            rc.words_uploaded += rc.mirror.size();
            rc.all_dirty = false;
        } else {
            size_t at = 0;
            for (const auto& [first, n] : rc.dirty) at += n;
            rc.words_uploaded += at;
        }
        rc.dirty.clear();
        rc.dirty_words.clear();
        g_vk_world.rcols_alloc = vk::alloc_ring(bytes, 16);
        if (!rc.mirror.empty()) std::memcpy(g_vk_world.rcols_alloc.ptr, rc.mirror.data(), rc.mirror.size() * 4);
        return;
    }
    if (g_rcol_buf == 0) glGenBuffers(1, &g_rcol_buf);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_rcol_buf, "MRP world: frame buffers");
    glBindBuffer(kGlShaderStorageBuffer, g_rcol_buf);
    if (rc.all_dirty || bytes != g_rcol_size) {
        glBufferData(kGlShaderStorageBuffer, static_cast<GLsizeiptr>(bytes), nullptr, GL_DYNAMIC_DRAW);
        if (!rc.mirror.empty()) glBufferSubData(kGlShaderStorageBuffer, 0, static_cast<GLsizeiptr>(rc.mirror.size() * 4), rc.mirror.data());
        g_rcol_size = bytes;
        rc.words_uploaded += rc.mirror.size();
        rc.all_dirty = false;
    } else {
        size_t at = 0;
        for (const auto& [first, n] : rc.dirty) {
            glBufferSubData(kGlShaderStorageBuffer, static_cast<GLintptr>(first) * 4, static_cast<GLsizeiptr>(n) * 4, rc.dirty_words.data() + at);
            at += n;
        }
        rc.words_uploaded += at;
    }
    rc.dirty.clear();
    rc.dirty_words.clear();
    glBindBufferBase(kGlShaderStorageBuffer, 3, g_rcol_buf);
}

// orig: (host), RAC1_MRPW_TFRAG_VERIFY: the compute program's vertices / triangles read back and compared with the CPU
// reference's (tfrag_expect): every field the world projection's vertex shader reads (the world point, STQ, the GS's Q, RGBA, F) bit
// for bit, the drawn triangles in order and their closing vertex's texture / GS state
void tfrag_verify(Frame& F) {
    std::vector<uint8_t> vb;
    std::vector<uint32_t> ib;
    tfrag_gpu_readback(F.tf, sizeof(GpuV), vb, ib);
    ++g_st.tv_frames;
    for (const Frame::TfragExpect& x : F.tf_expect) {
        const TfragGpuDraw* gd = nullptr;
        size_t di = 0;
        for (; di < F.tf.draws.size(); ++di)
            if (F.tf.draws[di].vbase == x.vbase) {
                gd = &F.tf.draws[di];
                break;
            }
        if (gd == nullptr) continue;
        const uint32_t nv = (di + 1 < F.tf.draws.size() ? F.tf.draws[di + 1].vbase : F.tf.nverts) - gd->vbase;
        const uint32_t ni = (di + 1 < F.tf.draws.size() ? F.tf.draws[di + 1].ibase : F.tf.nidx) - gd->ibase;
        g_st.tv_verts += nv;
        if (nv != x.nverts) {
            g_st.tv_vbad += nv;
            continue;
        }
        auto gpu = [&](uint32_t i) {
            GpuV v;
            std::memcpy(&v, vb.data() + static_cast<size_t>(gd->vbase + i) * sizeof(GpuV), sizeof(GpuV));
            return v;
        };
        static int shown = 0;
        for (uint32_t i = 0; i < nv; ++i) {
            const GpuV a = x.v[i], b = gpu(i);
            const bool d[6] = {std::memcmp(a.wp, b.wp, 16) != 0, std::memcmp(a.stq, b.stq, 12) != 0, std::memcmp(&a.lodq, &b.lodq, 4) != 0, std::memcmp(a.rgba, b.rgba, 4) != 0,
                               std::memcmp(&a.fog, &b.fog, 4) != 0, false};
            bool any = false;
            for (int k = 0; k < 5; ++k) {
                g_st.tv_field[k] += d[k] ? 1 : 0;
                any = any || d[k];
            }
            if (any) {
                ++g_st.tv_vbad;
                if (shown++ < 10)
                    std::printf("[mrpw tfrag] verify draw %zu vertex %u: wp %.9g %.9g %.9g %.9g / %.9g %.9g %.9g %.9g stq %.9g %.9g %.9g / %.9g %.9g %.9g lodq %.9g / %.9g rgba %02x%02x%02x%02x / "
                                "%02x%02x%02x%02x fog %g / %g\n",
                                di, i, a.wp[0], a.wp[1], a.wp[2], a.wp[3], b.wp[0], b.wp[1], b.wp[2], b.wp[3], a.stq[0], a.stq[1], a.stq[2], b.stq[0], b.stq[1], b.stq[2], a.lodq, b.lodq,
                                a.rgba[0], a.rgba[1], a.rgba[2], a.rgba[3], b.rgba[0], b.rgba[1], b.rgba[2], b.rgba[3], a.fog, b.fog);
            }
        }
        std::vector<uint32_t> got;
        for (uint32_t t = 0; t + 2 < ni; t += 3) {
            const uint32_t i0 = ib[gd->ibase + t], i1 = ib[gd->ibase + t + 1], i2 = ib[gd->ibase + t + 2];
            if (i0 == i1 && i1 == i2) continue;
            got.insert(got.end(), {i0 - gd->vbase, i1 - gd->vbase, i2 - gd->vbase});
        }
        g_st.tv_tris += x.tri.size() / 3;
        if (got != x.tri) {
            ++g_st.tv_tbad;
            if (shown++ < 10) std::printf("[mrpw tfrag] verify draw %zu: %zu triangles, the reference's %zu\n", di, got.size() / 3, x.tri.size() / 3);
            continue;
        }
        for (size_t t = 2; t < x.tri.size(); t += 3) {
            const GpuV a = x.v[x.tri[t]], b = gpu(x.tri[t]);
            constexpr uint32_t kUnit = 0xfu << 16;  // (the texture's unit: the GPU path's own, 0 in the CPU path's)
            if (a.word != b.word || a.layer != b.layer || (a.state[0] & ~kUnit) != (b.state[0] & ~kUnit) || a.state[1] != b.state[1]) {
                ++g_st.tv_field[5];
                if (shown++ < 10)
                    std::printf("[mrpw tfrag] verify draw %zu triangle %zu: word %x layer %u state %x %x / %x %u %x %x\n", di, t / 3, a.word, a.layer, a.state[0], a.state[1], b.word, b.layer,
                                b.state[0], b.state[1]);
            }
        }
    }
}

// orig: (host) a batch checked against the buffers its draw reads before it is submitted (the CPU path's indices, the
// compute program's tfrag indices, the GPU path's draw commands: their template indices / vertices and their records): a batch
// outside them is refused (logged), so that a data bug draws nothing instead of handing the GPU garbage. Null when it is fine.
const char* batch_refused(const Frame& F, const Level& L, const Batch& b) {
    const uint64_t end = static_cast<uint64_t>(b.first) + b.count;
    if (b.gpu) {
        if (end > F.draws.size()) return "draw commands";
        for (uint32_t i = b.first; i < end; ++i) {
            const DrawCmd& c = F.draws[i];
            if (static_cast<uint64_t>(c.first_index) + c.count > L.tidx.size()) return "template indices";
            if (c.base_vertex < 0 || static_cast<size_t>(c.base_vertex) > L.tverts.size()) return "template vertices";
            if (c.instances != 1 || (static_cast<uint64_t>(c.base_instance) + 1) * 8 > F.cmds.size()) return "draw records";
        }
        return nullptr;
    }
    if (b.tfrag) return end > F.tf.nidx ? "tfrag indices" : nullptr;
    return end > F.idx.size() ? "indices" : nullptr;
}

// (the modern lighting: `caster_vp` set, the frame's casters into the bound shadow map with that light view-projection, depth only)
// (`view` the shadow view a caster draw is for: 0..2 a cascade - the frame's draws and the casters sorted into it -, 3 + i
// point light slot i's cube - the casters near it alone)
void draw_frame(const gs::Viewport* vp, bool with_sky = true, const float* caster_vp = nullptr, int view = 0) {
    Frame& F = g_frame;
    Level& L = g_level;
    if (F.batches.empty()) return;
    const bool caster = caster_vp != nullptr;
    const bool upload = !F.uploaded;
    F.uploaded = true;

    if (vk::is_active()) {
        if (upload) {
            const size_t v_bytes = std::max<size_t>(F.v.size() * sizeof(GpuV), sizeof(GpuV));
            g_vk_world.cpu_vbo_alloc = vk::alloc_ring(v_bytes, 16);
            if (!F.v.empty()) std::memcpy(g_vk_world.cpu_vbo_alloc.ptr, F.v.data(), F.v.size() * sizeof(GpuV));

            const size_t i_bytes = std::max<size_t>(F.idx.size() * sizeof(uint32_t), sizeof(uint32_t));
            g_vk_world.cpu_ibo_alloc = vk::alloc_ring(i_bytes, 16);
            if (!F.idx.empty()) std::memcpy(g_vk_world.cpu_ibo_alloc.ptr, F.idx.data(), F.idx.size() * sizeof(uint32_t));

            if (!F.tf.draws.empty()) {
                tfrag_gpu_level(g_tlevel);
                tfrag_gpu_colours(g_tcols);
                tfrag_gpu_dispatch(F.tf, sizeof(GpuV));
                if (g_tfrag_verify) tfrag_verify(F);
            }
            upload_resident_colours();

            const size_t inst_bytes = std::max<size_t>(F.insts.size() * sizeof(InstGpu), 16);
            g_vk_world.insts_alloc = vk::alloc_ring(inst_bytes, 16);
            if (!F.insts.empty()) std::memcpy(g_vk_world.insts_alloc.ptr, F.insts.data(), F.insts.size() * sizeof(InstGpu));

            const size_t col_bytes = std::max<size_t>(F.scene.colours.size() * 16, 16);
            g_vk_world.cols_alloc = vk::alloc_ring(col_bytes, 16);
            if (!F.scene.colours.empty()) std::memcpy(g_vk_world.cols_alloc.ptr, F.scene.colours.data(), F.scene.colours.size() * 16);

            const size_t cmd_bytes = std::max<size_t>(F.cmds.size() * 4, 16);
            g_vk_world.cmds_alloc = vk::alloc_ring(cmd_bytes, 16);
            if (!F.cmds.empty()) std::memcpy(g_vk_world.cmds_alloc.ptr, F.cmds.data(), F.cmds.size() * 4);

            const size_t ind_bytes = std::max<size_t>(F.draws.size() * sizeof(DrawCmd), sizeof(DrawCmd));
            g_vk_world.indirect_alloc = vk::alloc_ring(ind_bytes, 16);
            if (!F.draws.empty()) std::memcpy(g_vk_world.indirect_alloc.ptr, F.draws.data(), F.draws.size() * sizeof(DrawCmd));
        }

        vk::ensure_rendering();
        VkCommandBuffer cb = vk::cmd();
        if (!caster) vk::set_scissor(false);

        const int mode = g_mrp_world_projection != 0 ? 1 : 0;
        const int mm = g_mrp_world_mip != 0 ? 1 : 0;
        const bool lit_prog = caster || mrp_lighting_active();

        WorldVkPush push{};
        mrp_world_view_proj(push.uVP);
        float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
        if (fb_w <= 0.0f) fb_w = 512.0f;
        if (fb_h <= 0.0f) fb_h = 416.0f;
        push.uFrame[0] = fb_w;
        push.uFrame[1] = fb_h;
        push.uFrame[2] = vp != nullptr ? vp->xscale_3d : 1.0f;
        push.uFrame[3] = 0.0f;
        double da = 0.0, db = 0.0;
        const bool has = render_gs_depth(da, db);
        const uint64_t xo = g_frame.env_xyoff;
        float x = static_cast<float>(xo & 0xffff), y = static_cast<float>((xo >> 32) & 0xffff);
        if (xo == 0) {
            x = (2048.0f - fb_w * 0.5f) * 16.0f;
            y = (2048.0f - fb_h * 0.5f) * 16.0f;
        }
        push.uXyOff[0] = x;
        push.uXyOff[1] = y;
        push.uXyOff[2] = has ? static_cast<float>(da) : 0.5f;
        push.uXyOff[3] = has ? static_cast<float>(db) : 0.0f;
        push.uMode = mode;
        push.uMipMode = g_mrp_world_mip;
        push.uShade = g_render_shade;
        push.uAtPass = 1;
        push.uU8 = 0;

        if (caster) {
            mrp_lighting_vk_bind_caster(reinterpret_cast<uint64_t>(g_vk_world.pipe_layout), caster_vp);
        }

        VkDescriptorImageInfo tex_infos[16]{};
        for (int u = 0; u < 16; ++u) {
            tex_infos[u].sampler = g_vk_world.samplers[mm][0];
            tex_infos[u].imageView = vk::white_texture_array().view;
            tex_infos[u].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        auto set_unit_info = [&](uint32_t u, int bucket, uint32_t sampler) {
            if (u >= 16) return;
            tex_infos[u].sampler = g_vk_world.samplers[mm][sampler & 15];
            tex_infos[u].imageView = (bucket >= 0 && static_cast<size_t>(bucket) < L.buckets.size() &&
                                      L.buckets[static_cast<size_t>(bucket)].vk_tex.view != VK_NULL_HANDLE)
                                         ? L.buckets[static_cast<size_t>(bucket)].vk_tex.view
                                         : vk::white_texture_array().view;
            tex_infos[u].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        };
        auto bind_set0 = [&] {
            VkDescriptorSet ds = vk::alloc_descriptor_set(g_vk_world.set0_layout);
            const vk::RingAlloc* bufs[4] = {
                &g_vk_world.insts_alloc,
                &g_vk_world.cols_alloc,
                &g_vk_world.cmds_alloc,
                &g_vk_world.rcols_alloc,
            };
            VkDescriptorBufferInfo bi[4]{};
            VkWriteDescriptorSet writes[5]{};
            for (uint32_t i = 0; i < 4; ++i) {
                bi[i].buffer = bufs[i]->buffer;
                bi[i].offset = bufs[i]->offset;
                bi[i].range = VK_WHOLE_SIZE;
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = ds;
                writes[i].dstBinding = i;
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writes[i].pBufferInfo = &bi[i];
            }
            writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[4].dstSet = ds;
            writes[4].dstBinding = 4;
            writes[4].descriptorCount = 16;
            writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[4].pImageInfo = tex_infos;
            vkUpdateDescriptorSets(vk::device(), 5, writes, 0, nullptr);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_world.pipe_layout, 0, 1, &ds, 0, nullptr);
        };

        const auto& rt_info = vk::current_target();
        const VkFormat cfmt = rt_info.color_view != VK_NULL_HANDLE ? rt_info.color_format : VK_FORMAT_UNDEFINED;
        const VkFormat dfmt = rt_info.depth_view != VK_NULL_HANDLE ? rt_info.depth_format : VK_FORMAT_UNDEFINED;
        const bool has_stencil = rt_info.stencil_view != VK_NULL_HANDLE;

        RKey cur;
        bool have = false;
        int cur_vb = -1, bound_epoch = -2, bound0_bucket = -2, cur_sky = -1;
        uint32_t bound0_sampler = 0xffffffffu;
        bool set0_bound = false;

        struct Pending {
            bool on = false;
            int kind = 0;  // 0 the CPU path, 1 the GPU path, 2 tfrag; + 3 the depth-only program
            uint32_t first = 0, count = 0;
            int epoch = -1, bucket = -2;
            uint32_t sampler = 0;
        } pend;
        auto flush = [&] {
            if (!pend.on) return;
            if (pend.kind % 3 == 1) {
                vkCmdDrawIndexedIndirect(cb, g_vk_world.indirect_alloc.buffer,
                                         g_vk_world.indirect_alloc.offset + static_cast<VkDeviceSize>(pend.first) * sizeof(DrawCmd),
                                         pend.count, sizeof(DrawCmd));
            } else {
                vkCmdDrawIndexed(cb, pend.count, 1, pend.first, 0, 0);
            }
            pend.on = false;
        };

        for (const Batch& b : F.batches) {
            if (b.sky && !with_sky) continue;
            if (caster && (b.sky || b.rk.zwrite == 0)) continue;
            static const bool show_casters = std::getenv("RAC1_SHADOW_SHOW_CASTERS") != nullptr;
            if (!caster && b.rk.shadow != show_casters && (b.gpu || b.rk.shadow)) continue;
            if (caster && b.rk.view_only) continue;
            if (caster && (b.rk.shadow ? ((b.rk.views >> view) & 1u) == 0 : view >= 3)) continue;
            static const int part = std::getenv("RAC1_SHADOW_REF_PART") != nullptr ? std::atoi(std::getenv("RAC1_SHADOW_REF_PART")) : 0;
            if (caster && part != 0 && (part == 1) == b.rk.shadow) continue;
            if (const char* why = batch_refused(F, L, b); why != nullptr) {
                static uint32_t shown = 0;
                if (shown++ < 8)
                    std::fprintf(stderr, "[mrpw] batch refused (%s): first %u count %u (frame: %zu indices, %u tfrag indices, %zu draw commands; level: %zu template indices)\n", why,
                                 b.first, b.count, F.idx.size(), F.tf.nidx, F.draws.size(), L.tidx.size());
                continue;
            }
            const Key& k = b.key;
            const RKey& rk = b.rk;
            static const bool no_early = std::getenv("RAC1_SHADOW_EARLYZ") != nullptr && std::atoi(std::getenv("RAC1_SHADOW_EARLYZ")) == 0;
            const bool early = caster && !b.ate && g_vk_world.fs_early != VK_NULL_HANDLE && !no_early;

            if (caster && pend.on) {
                const int kind = (b.gpu ? 1 : b.tfrag ? 2 : 0) + (early ? 3 : 0);
                const bool same = kind == pend.kind && b.first == pend.first + pend.count &&
                                  (kind % 3 == 0 ? k.bucket == pend.bucket && k.sampler == pend.sampler : (b.epoch < 0 || b.epoch == pend.epoch));
                if (same) {
                    pend.count += b.count;
                    continue;
                }
                flush();
            }

            if (!caster && cur_sky != (b.sky ? 1 : 0)) {
                cur_sky = b.sky ? 1 : 0;
                mrp_lighting_vk_bind_receiver(reinterpret_cast<uint64_t>(g_vk_world.pipe_layout), b.sky, lit_prog);
            }

            const int want_vb = b.gpu ? 1 : b.tfrag ? 2 : 0;
            if (want_vb != cur_vb) {
                if (want_vb == 1) {
                    VkDeviceSize vo = 0;
                    vkCmdBindVertexBuffers(cb, 0, 1, &L.vk_tvbo.buffer, &vo);
                    vkCmdBindIndexBuffer(cb, L.vk_tibo.buffer, 0, VK_INDEX_TYPE_UINT32);
                } else if (want_vb == 2) {
                    VkBuffer vbo = reinterpret_cast<VkBuffer>(tfrag_gpu_vk_vbo());
                    VkBuffer ibo = reinterpret_cast<VkBuffer>(tfrag_gpu_vk_ibo());
                    VkDeviceSize vo = 0;
                    vkCmdBindVertexBuffers(cb, 0, 1, &vbo, &vo);
                    vkCmdBindIndexBuffer(cb, ibo, 0, VK_INDEX_TYPE_UINT32);
                } else {
                    VkDeviceSize vo = g_vk_world.cpu_vbo_alloc.offset;
                    vkCmdBindVertexBuffers(cb, 0, 1, &g_vk_world.cpu_vbo_alloc.buffer, &vo);
                    vkCmdBindIndexBuffer(cb, g_vk_world.cpu_ibo_alloc.buffer, g_vk_world.cpu_ibo_alloc.offset, VK_INDEX_TYPE_UINT32);
                }
                cur_vb = want_vb;
            }

            bool set0_dirty = !set0_bound;
            if (b.gpu || b.tfrag) {
                if (b.epoch >= 0 && b.epoch != bound_epoch) {
                    const auto& ep = F.epochs[static_cast<size_t>(b.epoch)];
                    for (size_t u = 0; u < ep.size(); ++u) set_unit_info(static_cast<uint32_t>(u) + 1, ep[u].first, ep[u].second);
                    bound_epoch = b.epoch;
                    set0_dirty = true;
                }
            } else if (k.bucket != bound0_bucket || k.sampler != bound0_sampler) {
                set_unit_info(0, k.bucket, k.sampler);
                bound0_bucket = k.bucket;
                bound0_sampler = k.sampler;
                set0_dirty = true;
            }
            if (set0_dirty) {
                bind_set0();
                set0_bound = true;
            }

            if (!caster && (!have || rk.abe != cur.abe || rk.alpha != cur.alpha)) {
                gs::DrawState ds{};
                ds.prim = k.prim;
                ds.alpha = k.alpha;
                gs::blend_state(ds);
            }

            push.uFogCol_Pad[0] = static_cast<float>(rk.fogcol & 0xff) / 255.0f;
            push.uFogCol_Pad[1] = static_cast<float>((rk.fogcol >> 8) & 0xff) / 255.0f;
            push.uFogCol_Pad[2] = static_cast<float>((rk.fogcol >> 16) & 0xff) / 255.0f;
            push.uFge = (rk.fogcol >> 24) & 1 ? 1 : 0;

            const uint32_t ztst = fld(k.test, 17, 2);
            const bool ztest = caster ? true : (fld(k.test, 16, 1) != 0 && ztst != 1);
            const VkCompareOp zcmp = caster ? VK_COMPARE_OP_LESS_OR_EQUAL
                                            : (ztst == 0 ? VK_COMPARE_OP_NEVER : ztst == 2 ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS);
            const bool zwrite = rk.zwrite != 0;

            WorldVkPipeKey pk{};
            pk.gpu = b.gpu;
            pk.lit = lit_prog;
            pk.early = early;
            pk.caster = caster;
            pk.depth_clamp = caster ? false : (mode == 1 || b.sky);
            pk.depth_test = ztest;
            pk.depth_compare = zcmp;
            pk.prim = k.prim & (1ull << 6);
            pk.alpha = pk.prim ? k.alpha : 0;
            pk.color_fmt = cfmt;
            pk.depth_fmt = dfmt;
            pk.has_stencil = has_stencil;

            auto bind_pass = [&](int pass, bool dmask, int cmask) {
                push.uAtPass = pass;
                pk.depth_write = dmask;
                pk.depth_compare = (g_p2_less && lit_prog && pass == 2 && !dmask && zcmp == VK_COMPARE_OP_LESS_OR_EQUAL) ? VK_COMPARE_OP_LESS : zcmp;
                pk.color_mask = cmask == 0 ? 0xF : cmask == 2 ? 0 : 0x7;
                vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, get_world_vk_pipeline(pk));
                vkCmdPushConstants(cb, g_vk_world.pipe_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
            };
            auto draw_vk = [&] {
                if (b.gpu) {
                    vkCmdDrawIndexedIndirect(cb, g_vk_world.indirect_alloc.buffer,
                                             g_vk_world.indirect_alloc.offset + static_cast<VkDeviceSize>(b.first) * sizeof(DrawCmd),
                                             b.count, sizeof(DrawCmd));
                } else {
                    vkCmdDrawIndexed(cb, b.count, 1, b.first, 0, 0);
                }
            };

            if (caster) {
                bind_pass(1, true, 2);
                pend.on = true;
                pend.kind = (b.gpu ? 1 : b.tfrag ? 2 : 0) + (early ? 3 : 0);
                pend.first = b.first;
                pend.count = b.count;
                pend.epoch = b.epoch;
                pend.bucket = k.bucket;
                pend.sampler = k.sampler;
                cur = rk;
                have = true;
                continue;
            }
            bind_pass(1, zwrite, 0);
            draw_vk();
            static const bool one_pass = std::getenv("RAC1_MRPW_ONEPASS") != nullptr;
            if (rk.pass2 != 0 && !one_pass) {
                const uint32_t afail = rk.pass2;
                bind_pass(2, afail == 2 && zwrite, afail == 2 ? 2 : afail == 3 ? 3 : 0);
                draw_vk();
            }
            cur = rk;
            have = true;
        }
        flush();
        return;
    }

    const VariantScope variant(caster || mrp_lighting_active());  // (the shadow pass too: its early exit)
    // (the modern lighting draws the frame several times: its data uploaded once, by the first)
    if (upload && !F.idx.empty()) {
        glBindVertexArray(L.vao);
        glBindBuffer(GL_ARRAY_BUFFER, L.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(F.v.size() * sizeof(GpuV)), F.v.data(), GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, L.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(F.idx.size() * sizeof(uint32_t)), F.idx.data(), GL_STREAM_DRAW);
    }
    auto ssbo = [](GLuint b, int binding, const void* data, size_t bytes) {
        glBindBuffer(kGlShaderStorageBuffer, b);
        glBufferData(kGlShaderStorageBuffer, static_cast<GLsizeiptr>(std::max<size_t>(bytes, 16)), nullptr, GL_STREAM_DRAW);
        if (bytes > 0) glBufferSubData(kGlShaderStorageBuffer, 0, static_cast<GLsizeiptr>(bytes), data);
        glBindBufferBase(kGlShaderStorageBuffer, static_cast<GLuint>(binding), b);
    };
    if (upload && !F.tf.draws.empty()) {  // the compute program writes the frame's tfrag vertices / triangles (first: it binds the storage buffers of its own)
        tfrag_gpu_level(g_tlevel);
        tfrag_gpu_colours(g_tcols);
        tfrag_gpu_dispatch(F.tf, sizeof(GpuV));
        if (g_tfrag_vao == 0) {
            glGenVertexArrays(1, &g_tfrag_vao);
            glBindVertexArray(g_tfrag_vao);
            glBindBuffer(GL_ARRAY_BUFFER, tfrag_gpu_vbo());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tfrag_gpu_ibo());
            const GLsizei S = sizeof(GpuV);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, wp)));
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, vu)));
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, stq)));
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, lodq)));
            glVertexAttribIPointer(4, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(GpuV, rgba)));
            glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(GpuV, fog)));
            glVertexAttribIPointer(6, 2, GL_UNSIGNED_INT, S, reinterpret_cast<void*>(offsetof(GpuV, word)));
            glVertexAttribIPointer(7, 2, GL_UNSIGNED_INT, S, reinterpret_cast<void*>(offsetof(GpuV, state)));
            for (GLuint a = 0; a < 8; ++a) glEnableVertexAttribArray(a);
            glBindVertexArray(0);
        }
        if (g_tfrag_verify) tfrag_verify(F);
    }
    if (upload) {
        upload_resident_colours();
        ssbo(g_ssbo[0], 0, F.insts.data(), F.insts.size() * sizeof(InstGpu));
        ssbo(g_ssbo[1], 1, F.scene.colours.data(), F.scene.colours.size() * 16);
        ssbo(g_ssbo[2], 2, F.cmds.data(), F.cmds.size() * 4);
        glBindBuffer(kGlShaderStorageBuffer, 0);
        glBindBuffer(kGlDrawIndirectBuffer, g_indirect);
        glBufferData(kGlDrawIndirectBuffer, static_cast<GLsizeiptr>(std::max<size_t>(F.draws.size() * sizeof(DrawCmd), 16)), F.draws.data(), GL_STREAM_DRAW);
        uint32_t ids = 0;  // (the draws' base instances within the draw indices' buffer)
        for (const DrawCmd& d : F.draws) ids = std::max(ids, d.base_instance + 1);
        draw_ids_reserve(ids);
    } else {  // (the buffers bound again: another renderer's draw may have bound its own since)
        for (int i = 0; i < 3; ++i) glBindBufferBase(kGlShaderStorageBuffer, static_cast<GLuint>(i), g_ssbo[i]);
        glBindBufferBase(kGlShaderStorageBuffer, 3, g_rcol_buf);
        glBindBuffer(kGlDrawIndirectBuffer, g_indirect);
    }
    const int mode = g_mrp_world_projection != 0 ? 1 : 0;
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    const GLboolean depth_clamp = glIsEnabled(GL_DEPTH_CLAMP);
    int clamp_on = -1;  // (the VU projection; the sky's directions in the world projection: at the far plane)
    // (RAC1_FRAME_PROF: the sky's batches, first in the frame, timed on the GPU by a timer query)
    static GLuint sky_query = 0;
    bool sky_timing = false;
    static const bool sky_prof = g_env_prof && std::atoi(std::getenv("RAC1_FRAME_PROF")) <= 1;  // (RAC1_FRAME_PROF=2 times whole stages)
    if (sky_prof && with_sky && !F.batches.empty() && F.batches.front().sky) {
        if (sky_query == 0) glGenQueries(1, &sky_query);
        glBeginQuery(GL_TIME_ELAPSED, sky_query);
        sky_timing = true;
    }
    const int mm = g_mrp_world_mip != 0 ? 1 : 0;
    // the GL state of the batch key (RKey) set when it changes; the textures: unit 0 the CPU path's batch's, units 1. the GPU
    // path's epoch's
    RKey cur;
    bool have = false;
    int cur_path = -1, last_path = -1, bound_epoch = -2, bound0_bucket = -2;
    GLuint cur_vao = 0xffffffffu;  // (the tfrag GPU path's batches read the compute program's buffers)
    int gl_pass[4] = {-1, -1, -1, -1}, gl_dmask = -1, gl_cmask = -1;
    uint32_t bound0_sampler = 0xffffffffu, bound_units = 0;
    auto bind_unit = [&](uint32_t u, int bucket, uint32_t sampler) {
        glActiveTexture(GL_TEXTURE0 + u);
        glBindTexture(GL_TEXTURE_2D_ARRAY, bucket >= 0 ? L.buckets[static_cast<size_t>(bucket)].tex : 0);
        glBindSampler(u, g_samplers[mm][sampler & 15]);
    };
    int cur_sky = -1;
    // (the shadow pass merges consecutive batches whose draws are contiguous and share what a depth-only draw needs - the path,
    // the texture units / texture: the depth state is one; the per-batch GL state of the view - blending, depth tests - not needed)
    struct Pending {
        bool on = false;
        int kind = 0;  // 0 the CPU path, 1 the GPU path, 2 tfrag; + 3 the depth-only program
        uint32_t first = 0, count = 0;
        int epoch = -1, bucket = -2;
        uint32_t sampler = 0;
    } pend;
    auto flush = [&] {
        if (!pend.on) return;
        if (pend.kind % 3 == 1)
            p_multi_draw_elements_indirect(GL_TRIANGLES, GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<uintptr_t>(pend.first) * sizeof(DrawCmd)), static_cast<GLsizei>(pend.count), 0);
        else
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(pend.count), GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<uintptr_t>(pend.first) * sizeof(uint32_t)));
        pend.on = false;
    };
    for (const Batch& b : F.batches) {
        if (b.sky && !with_sky) continue;
        if (caster && (b.sky || b.rk.zwrite == 0)) continue;  // (the casters: what writes depth, not the sky)
        static const bool show_casters = std::getenv("RAC1_SHADOW_SHOW_CASTERS") != nullptr;  // (testing: the casters' set drawn in the view instead)
        if (!caster && b.rk.shadow != show_casters && (b.gpu || b.rk.shadow)) continue;  // (the casters outside the view, the shadow maps' only)
        if (caster && b.rk.view_only) continue;  // (the frame's shrubs: the casters' own models cast)
        if (caster && (b.rk.shadow ? ((b.rk.views >> view) & 1u) == 0 : view >= 3)) continue;  // (the view's own casters)
        static const int part = std::getenv("RAC1_SHADOW_REF_PART") != nullptr ? std::atoi(std::getenv("RAC1_SHADOW_REF_PART")) : 0;  // (testing: 1 the frame's casters, 2 the added ones)
        if (caster && part != 0 && (part == 1) == b.rk.shadow) continue;
        if (const char* why = batch_refused(F, L, b); why != nullptr) {
            static uint32_t shown = 0;
            if (shown++ < 8)
                std::fprintf(stderr, "[mrpw] batch refused (%s): first %u count %u (frame: %zu indices, %u tfrag indices, %zu draw commands; level: %zu template indices)\n", why,
                             b.first, b.count, F.idx.size(), F.tf.nidx, F.draws.size(), L.tidx.size());
            continue;
        }
        const Key& k = b.key;
        const RKey& rk = b.rk;
        static const bool no_early = std::getenv("RAC1_SHADOW_EARLYZ") != nullptr && std::atoi(std::getenv("RAC1_SHADOW_EARLYZ")) == 0;  // (testing)
        const bool early = caster && !b.ate && g_prog_e != 0 && !no_early;  // (an opaque caster batch: the depth-only program)
        const Loc& lc = early ? (b.gpu ? g_loc_gpu_e : g_loc_e) : (b.gpu ? g_loc_gpu : g_loc);
        if (caster && pend.on) {
            const int kind = (b.gpu ? 1 : b.tfrag ? 2 : 0) + (early ? 3 : 0);
            const bool same = kind == pend.kind && b.first == pend.first + pend.count &&
                              (kind % 3 == 0 ? k.bucket == pend.bucket && k.sampler == pend.sampler : (b.epoch < 0 || b.epoch == pend.epoch));
            if (same) {
                pend.count += b.count;
                continue;
            }
            flush();
        }
        if (sky_timing && !b.sky) {
            glEndQuery(GL_TIME_ELAPSED);
            sky_timing = false;
            GLuint64 ns = 0;
            glGetQueryObjectui64v(sky_query, GL_QUERY_RESULT, &ns);
            g_st.ms_sky += static_cast<double>(ns) * 1e-6;
        }
        if (const int want = caster ? 0 : mode == 1 || b.sky ? 1 : 0; want != clamp_on) {  // (the casters not clamped, mrp_lighting.cpp kBack)
            if (want != 0) glEnable(GL_DEPTH_CLAMP);
            else glDisable(GL_DEPTH_CLAMP);
            clamp_on = want;
        }
        if (cur_path != (b.gpu ? 1 : 0) + (early ? 2 : 0)) {
            cur_path = (b.gpu ? 1 : 0) + (early ? 2 : 0);
            glUseProgram(early ? (b.gpu ? g_prog_gpu_e : g_prog_e) : (b.gpu ? g_prog_gpu : g_prog));
            set_frame_uniforms(lc, vp, mode);
            if (caster) mrp_lighting_bind_caster(lc.sh, caster_vp);
            cur_sky = -1;
        }
        if (!caster && cur_sky != (b.sky ? 1 : 0)) {  // (the modern lighting's receivers: not the sky)
            cur_sky = b.sky ? 1 : 0;
            mrp_lighting_bind_receiver(lc.sh, b.sky);
        }
        if (const GLuint want = b.gpu ? L.tvao : b.tfrag ? g_tfrag_vao : L.vao; want != cur_vao) {
            glBindVertexArray(want);
            cur_vao = want;
        }
        if (b.gpu || b.tfrag) {  // (the tfrag GPU path samples by unit too)
            if (b.epoch >= 0 && b.epoch != bound_epoch) {
                const auto& ep = F.epochs[static_cast<size_t>(b.epoch)];
                for (size_t u = 0; u < ep.size(); ++u) bind_unit(static_cast<uint32_t>(u) + 1, ep[u].first, ep[u].second);
                bound_units = std::max(bound_units, static_cast<uint32_t>(ep.size()));
                bound_epoch = b.epoch;
            }
        } else if (k.bucket != bound0_bucket || k.sampler != bound0_sampler) {
            bind_unit(0, k.bucket, k.sampler);
            bound0_bucket = k.bucket;
            bound0_sampler = k.sampler;
        }
        if (!caster && (!have || rk.abe != cur.abe || rk.alpha != cur.alpha)) {
            gs::DrawState ds{};
            ds.prim = k.prim;
            ds.alpha = k.alpha;
            gs::blend_state(ds);
        }
        if (!have || rk.fogcol != cur.fogcol || cur_path != last_path) {
            glUniform3f(lc.fogcol, static_cast<float>(rk.fogcol & 0xff) / 255.0f, static_cast<float>((rk.fogcol >> 8) & 0xff) / 255.0f, static_cast<float>((rk.fogcol >> 16) & 0xff) / 255.0f);
            glUniform1i(lc.fge, (rk.fogcol >> 24) & 1 ? 1 : 0);
            last_path = cur_path;
        }
        if (!caster && (!have || rk.ztest != cur.ztest)) {
            const uint32_t ztst = fld(k.test, 17, 2);
            if (fld(k.test, 16, 1) != 0 && ztst != 1) {  // the GS's GEQUAL / GREATER on the host's depth: LEQUAL / LESS
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(ztst == 0 ? GL_NEVER : ztst == 2 ? GL_LEQUAL : GL_LESS);
            } else {
                glDisable(GL_DEPTH_TEST);
            }
        }
        const bool zwrite = rk.zwrite != 0;
        auto draw = [&] {
            if (b.gpu)
                p_multi_draw_elements_indirect(GL_TRIANGLES, GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<uintptr_t>(b.first) * sizeof(DrawCmd)), static_cast<GLsizei>(b.count), 0);
            else
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(b.count), GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<uintptr_t>(b.first) * sizeof(uint32_t)));
        };
        // the alpha test (per draw, draw_flags): pass 1 draws the passing pixels of the draws whose test is on (its mode: on, not ALWAYS,
        // and not AFAIL FB_ONLY without a depth write - that draws the failing pixels as the passing ones: one pass, in the primitives'
        // order as the GS blends them; e.g. the sky's shells) and every pixel of the others; pass 2 the failing ones as AFAIL
        // says (RKey::pass2: 1 FB_ONLY the colour with alpha, 2 ZB_ONLY the depth, 3 RGB_ONLY the colour without alpha; 0 KEEP nothing)
        // (the pass uniform, depth mask and colour mask set only when they change)
        auto set_pass = [&](int pass, bool dmask, int cmask) {
            if (pass != gl_pass[cur_path]) {
                glUniform1i(lc.atpass, pass);
                gl_pass[cur_path] = pass;
            }
            if (static_cast<int>(dmask) != gl_dmask) {
                glDepthMask(dmask ? GL_TRUE : GL_FALSE);
                gl_dmask = dmask ? 1 : 0;
            }
            if (cmask != gl_cmask) {
                if (cmask == 0) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                else if (cmask == 2) glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                else glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
                gl_cmask = cmask;
            }
        };
        if (caster) {
            set_pass(1, true, 2);
            pend.on = true;
            pend.kind = (b.gpu ? 1 : b.tfrag ? 2 : 0) + (early ? 3 : 0);
            pend.first = b.first;
            pend.count = b.count;
            pend.epoch = b.epoch;
            pend.bucket = k.bucket;
            pend.sampler = k.sampler;
            cur = rk;
            have = true;
            continue;
        }
        set_pass(1, zwrite, 0);
        draw();
        static const bool one_pass = std::getenv("RAC1_MRPW_ONEPASS") != nullptr;  // (testing: the failing pixels not drawn)
        if (rk.pass2 != 0 && !one_pass) {
            const uint32_t afail = rk.pass2;
            const bool less = g_p2_less && variant.on && !(afail == 2 && zwrite) && fld(k.test, 16, 1) != 0 && fld(k.test, 17, 2) == 2;  // (as the Vulkan path: g_p2_less)
            if (less) glDepthFunc(GL_LESS);
            set_pass(2, afail == 2 && zwrite, afail == 2 ? 2 : afail == 3 ? 3 : 0);
            draw();
            if (less) glDepthFunc(GL_LEQUAL);
        }
        cur = rk;
        have = true;
    }
    flush();  // (the shadow pass's last merged draw)
    if (sky_timing) {  // (a frame of sky alone)
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(sky_query, GL_QUERY_RESULT, &ns);
        g_st.ms_sky += static_cast<double>(ns) * 1e-6;
    }
    if (gl_cmask != 0) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    for (uint32_t u = 0; u <= bound_units; ++u) {
        glActiveTexture(GL_TEXTURE0 + u);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glBindSampler(u, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    if (depth_clamp) glEnable(GL_DEPTH_CLAMP);
    else glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glBindVertexArray(0);
    glBindBuffer(kGlDrawIndirectBuffer, 0);
    glUseProgram(0);
}

int program_index(uint64_t hash) {
    int n = 0;
    const WorldVu1Program* p = world_vu1_programs(n);
    for (int i = 0; i < n; ++i)
        if (p[i].hash == hash) return i;
    return -1;
}

}  // namespace

// orig: (host) see mrp_world_renderer.h
bool mrp_world_ready() { return g_world_mrp && (vk::is_active() ? g_vk_world.inited : g_prog != 0) && g_level.loaded && g_level.id == world_level_id(); }

// orig: (host) see mrp_world_renderer.h
bool mrp_world_active() { return g_frame.active; }

// orig: (host) see mrp_world_renderer.h
bool mrp_world_static_lod() { return g_static_on && !g_static_check && mrp_world_ready() && g_slevel.ok && g_mrp_world_projection == 0 && !g_env_check && !g_env_diff; }

// orig: (host) see mrp_world_renderer.h
bool mrp_world_tfrag_lod() {
    return tfrag_gpu_enabled() && tfrag_host_enabled() && !g_tfrag_check && mrp_world_ready() && g_tlevel.ok && tfrag_gpu_ready() && g_mrp_world_projection == 0 && !g_env_check && !g_env_diff;
}

// orig: (host) see mrp_world_renderer.h
bool mrp_world_testing() { return g_world_mrp && (g_env_check || g_env_diff); }

namespace {
// orig: (host) the scene's worker: one thread, one job a frame (scene_build reads EE memory, the level's assets and
// its own walker; the host thread meanwhile runs the GS replica's chain, which only reads EE memory)
// ---- the automatic emitters: the ties whose textures glow -----------------------------------------------------------
//
// Our art choice, not the game's: a tie class's triangles textured with bright, saturated texels (lamps, glowing
// panels: value >= 0.8, saturation >= 0.55, most of the triangle's samples) grouped per class into lights in the model's frame (a
// 4-unit grid, the class's 12 largest cells: their area-weighted centre, mean colour, area), then placed at every instance of the class (its matrix and position
// in the level's tables) as the level's static list of point lights, lit per pixel with the game's (mrp_lighting.cpp).
struct Emitter {
    float pos[3] = {}, r = 0.0f;
    float col[3] = {};
};
std::vector<Emitter> g_emitters;
std::vector<EmitterClass> g_emitter_classes;  // (mrp_world_emitter_classes)
int g_emitters_level = -2;

bool emissive_rgba(uint32_t c) {
    const float r = static_cast<float>(c & 0xff) / 255.0f, g = static_cast<float>((c >> 8) & 0xff) / 255.0f, b = static_cast<float>((c >> 16) & 0xff) / 255.0f;
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    if (!(mx >= 0.8f && mx > 0.0f && (mx - mn) / mx >= 0.55f && ((c >> 24) & 0xff) >= 0x40)) return false;
    // Warm diffuse palettes (ochre rock, orange wood, rust, dirt: R dominant, low B, moderate G) often have V in 0.80..0.90;
    // require genuinely bright emission on warm texels so terrain highlights and wooden beams are not mistaken for bulbs.
    const bool warm = r >= g && g >= b && b < 0.35f;
    if (warm && mx < 0.92f && (0.299f * r + 0.587f * g + 0.114f * b) < 0.72f) return false;
    return true;
}

// orig: (host) a colour's hue, degrees (0 red, 120 green, 240 blue)
float hue_of(double r, double g, double b) {
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
    if (!(mx - mn > 1e-6)) return 0.0f;
    const double h = mx == r ? std::fmod((g - b) / (mx - mn), 6.0) : mx == g ? (b - r) / (mx - mn) + 2.0 : (r - g) / (mx - mn) + 4.0;
    return static_cast<float>(h < 0.0 ? h * 60.0 + 360.0 : h * 60.0);
}

void build_emitters() {
    const Level& L = g_level;
    const double t0 = now_ms();
    g_emitters.clear();
    g_emitter_classes.clear();
    g_emitters_level = L.id;
    if (!L.loaded || L.world == nullptr || !g_slevel.ok) return;
    auto csm1 = [](uint32_t i) { return ((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i; };
    // the packets' classes
    std::unordered_map<const TiePacket*, const TieClass*> cls_of;
    for (const TieClass& c : L.world->ties)
        for (const TiePacket& p : c.packets) cls_of[&p] = &c;
    struct Cell {
        double w = 0, p[3] = {}, c[3] = {};
    };
    std::unordered_map<const TieClass*, std::vector<Emitter>> model;  // per class: lights in the model's frame (pos: model units; r: area)
    for (const StaticTieClass& sc : g_slevel.ties) {
        if (sc.lod[0].empty() || sc.lod[0].front().tie == nullptr) continue;
        const auto ci = cls_of.find(sc.lod[0].front().tie);
        if (ci == cls_of.end() || model.count(ci->second) != 0) continue;
        const TieClass& tc = *ci->second;
        std::map<std::tuple<int, int, int>, Cell> cells;
        double total_area = 0.0, lit_area = 0.0;  // (the class's whole surface, its glowing part)
        double white_area = 0.0;                  // (testing, RAC1_LIGHTS_WHITE: the part textured near white - neon panels)
        static const bool white_trace = std::getenv("RAC1_LIGHTS_WHITE") != nullptr;
        for (const StaticPacket& sp : sc.lod[0]) {
            const auto ti = L.tpl.find(sp.tie);
            if (ti == L.tpl.end()) continue;
            const Tpl& t = ti->second;
            for (const TplRun& run : t.runs) {
                if (run.slot < 0 || static_cast<size_t>(run.slot) >= sp.tie->prims.size()) continue;
                const int prim = sp.tie->prims[static_cast<size_t>(run.slot)];
                if (prim < 0 || static_cast<size_t>(prim) >= tc.textures.size()) continue;
                const int ti2 = tc.textures[static_cast<size_t>(prim)].texture;
                if (ti2 < 0 || static_cast<size_t>(ti2) >= L.tex[1].size()) continue;
                const mrp::Texture& tx = L.tex[1][static_cast<size_t>(ti2)];
                if (tx.width == 0 || tx.mips.empty() || tx.mips[0].size() < static_cast<size_t>(tx.width) * tx.height) continue;
                auto texel = [&](float s, float tt) {
                    const float fs = s - std::floor(s), ft = tt - std::floor(tt);
                    const uint32_t x = std::min(tx.width - 1, static_cast<uint32_t>(fs * static_cast<float>(tx.width)));
                    const uint32_t y = std::min(tx.height - 1, static_cast<uint32_t>(ft * static_cast<float>(tx.height)));
                    return tx.palette[csm1(tx.mips[0][static_cast<size_t>(y) * tx.width + x])];
                };
                for (uint32_t k = 0; k + 2 < run.count; k += 3) {
                    const TVert* v[3];
                    for (int j = 0; j < 3; ++j) v[j] = &L.tverts[t.first_vertex + L.tidx[t.first_index + run.first + k + static_cast<uint32_t>(j)]];
                    float s[3], tt[3], p[3][3];
                    for (int j = 0; j < 3; ++j) {
                        s[j] = static_cast<float>(v[j]->uv[0]) / 4096.0f;
                        tt[j] = static_cast<float>(v[j]->uv[1]) / 4096.0f;
                        for (int l = 0; l < 3; ++l) p[j][l] = static_cast<float>(v[j]->pos[l]);
                    }
                    // seven samples: the corners, the edges' middles, the centre
                    static const float bw[7][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {.5f, .5f, 0}, {0, .5f, .5f}, {.5f, 0, .5f}, {1 / 3.f, 1 / 3.f, 1 / 3.f}};
                    int hits = 0, whites = 0;
                    double cr = 0, cg = 0, cb = 0;
                    for (const auto& b : bw) {
                        const uint32_t c = texel(b[0] * s[0] + b[1] * s[1] + b[2] * s[2], b[0] * tt[0] + b[1] * tt[1] + b[2] * tt[2]);
                        if (white_trace) {  // (near white: bright and grey)
                            const int r8 = static_cast<int>(c & 0xff), g8 = static_cast<int>((c >> 8) & 0xff), b8 = static_cast<int>((c >> 16) & 0xff);
                            const int mx = std::max({r8, g8, b8}), mn = std::min({r8, g8, b8});
                            if (mn >= 0xb0 && mx - mn <= mx / 6) ++whites;
                        }
                        if (!emissive_rgba(c)) continue;
                        ++hits;
                        cr += (c & 0xff) / 255.0;
                        cg += ((c >> 8) & 0xff) / 255.0;
                        cb += ((c >> 16) & 0xff) / 255.0;
                    }
                    const double e1[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]}, e2[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
                    const double cx = e1[1] * e2[2] - e1[2] * e2[1], cy = e1[2] * e2[0] - e1[0] * e2[2], cz = e1[0] * e2[1] - e1[1] * e2[0];
                    const double tri = 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz) / (1024.0 * 1024.0);  // (world units^2 at scale 1)
                    if (std::isfinite(tri)) total_area += tri;
                    if (whites >= 3 && std::isfinite(tri)) white_area += tri * whites / 7.0;
                    if (hits < 4) continue;
                    const double area = tri * hits / 7.0;
                    if (!(area > 0.0)) continue;
                    lit_area += area;
                    double ctr[3];
                    for (int l = 0; l < 3; ++l) ctr[l] = (p[0][l] + p[1][l] + p[2][l]) / 3.0;
                    Cell& cell = cells[{static_cast<int>(std::floor(ctr[0] / 4096.0)), static_cast<int>(std::floor(ctr[1] / 4096.0)), static_cast<int>(std::floor(ctr[2] / 4096.0))}];
                    cell.w += area;
                    for (int l = 0; l < 3; ++l) cell.p[l] += ctr[l] * area;
                    cell.c[0] += cr / hits * area;
                    cell.c[1] += cg / hits * area;
                    cell.c[2] += cb / hits * area;
                }
            }
        }
        std::vector<Emitter>& out = model[&tc];
        // a class glowing over much of its surface in a warm colour is a bright material - Eudora's orange trees and woodwork,
        // Rilgar's red and yellow paint -, not a lamp (a lamp's bulb, a panel's strip: a small part of its object). A cool one (cyan, blue:
        // screens, glass, energy - Veldin's house panels) stays a light unless nearly the whole large mesh is painted that colour.
        static const double max_share = std::getenv("RAC1_EMIT_MAX_SHARE") != nullptr ? std::atof(std::getenv("RAC1_EMIT_MAX_SHARE")) : 0.15;  // (testing)
        const double share = total_area > 0.0 ? lit_area / total_area : 1.0;
        double mc[3] = {0, 0, 0}, mw = 0;
        for (const auto& [key, cell] : cells) {
            for (int l = 0; l < 3; ++l) mc[l] += cell.c[l];
            mw += cell.w;
        }
        const float hue = mw > 0.0 ? hue_of(mc[0] / mw, mc[1] / mw, mc[2] / mw) : 0.0f;  // (degrees, of the glowing part's mean colour)
        const bool cool = hue >= 165.0f && hue <= 260.0f;
        const bool material = (!cool && (share > max_share || lit_area > 220.0)) || (cool && share > 0.85 && total_area > 500.0);
        if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr && lit_area > 0.0)
            std::printf("[lights] tie class %d: glowing %.1f of %.1f units^2 (%.0f %%), hue %.0f%s\n", tc.o_class, lit_area, total_area, 100.0 * share, hue,
                        material ? ": a material, not a lamp" : "");
        if (white_trace && white_area > 0.0)
            std::printf("[lights] tie class %d: near white %.2f of %.1f units^2 (%.1f %%), %u instances\n", tc.o_class, white_area, total_area, 100.0 * white_area / total_area,
                        lhu(sc.rec + 0x26));
        if (lit_area > 0.0) g_emitter_classes.push_back(EmitterClass{tc.o_class, static_cast<float>(share), hue, 0, material});
        if (material || mrp_user_lights_class_off(tc.o_class)) cells.clear();
        // (the 12 largest cells of the class: a lamp is a few lights, a glowing wall a dozen at most)
        std::vector<const Cell*> order;
        for (const auto& [key, cell] : cells) order.push_back(&cell);
        std::sort(order.begin(), order.end(), [](const Cell* a, const Cell* b) { return a->w > b->w; });
        if (order.size() > 12) order.resize(12);
        for (const Cell* cp : order) {
            const Cell& cell = *cp;
            if (cell.w < 0.05) continue;  // (a speck)
            Emitter e;
            for (int l = 0; l < 3; ++l) {
                e.pos[l] = static_cast<float>(cell.p[l] / cell.w);
                e.col[l] = static_cast<float>(cell.c[l] / cell.w);
            }
            e.r = static_cast<float>(cell.w);
            out.push_back(e);
        }
    }
    // the instances: world = position + rows x scale x model / 1024 (as the GPU path's vertex shader with the instance's rows)
    size_t classes = 0;
    for (const StaticTieClass& sc : g_slevel.ties) {
        if (sc.lod[0].empty() || sc.lod[0].front().tie == nullptr) continue;
        const auto ci = cls_of.find(sc.lod[0].front().tie);
        if (ci == cls_of.end()) continue;
        const std::vector<Emitter>& em = model[ci->second];
        if (em.empty()) continue;
        ++classes;
        if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr) {  // (testing: the emitting classes)
            double area = 0;
            for (const Emitter& e : em) area += e.r;
            std::printf("[lights] tie class %d (rec %08x): %zu lights, area %.2f (model), %u instances; first colour %.2f %.2f %.2f\n", ci->second->o_class, sc.rec, em.size(), area,
                        lhu(sc.rec + 0x26), em[0].col[0], em[0].col[1], em[0].col[2]);
        }
        const uint32_t flags = lhu(sc.rec + 0x24);
        if ((flags & 9) != 0) continue;
        const uint32_t bounds = lwu(sc.rec + 0x28), n = lhu(sc.rec + 0x26);
        for (EmitterClass& ec : g_emitter_classes)
            if (ec.o_class == ci->second->o_class) ec.lights += static_cast<int>(em.size() * n);
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t data = lwu(bounds + 0x20 * i + 0x10);
            float m[3][3], pos[4];
            for (uint32_t l = 0; l < 4; ++l) pos[l] = lwc1(data + 0x30 + 4 * l);
            for (uint32_t r = 0; r < 3; ++r)
                for (uint32_t l = 0; l < 3; ++l) m[r][l] = lwc1(data + 0x10 * r + 4 * l) * pos[3];
            for (const Emitter& e : em) {
                Emitter w;
                for (int l = 0; l < 3; ++l) w.pos[l] = pos[l] + (m[0][l] * e.pos[0] + m[1][l] * e.pos[1] + m[2][l] * e.pos[2]) / 1024.0f;
                const float scale = std::fabs(pos[3]) > 0.0f ? std::fabs(pos[3]) : 1.0f;
                const float area = e.r * scale * scale;
                w.r = std::clamp(2.0f + 3.0f * std::sqrt(area), 2.0f, 7.0f);
                static const float gain = std::getenv("RAC1_AUTO_LIGHTS_GAIN") != nullptr ? static_cast<float>(std::atof(std::getenv("RAC1_AUTO_LIGHTS_GAIN"))) : 1.0f;  // (testing)
                const float k = gain * std::clamp(0.35f + 0.25f * std::sqrt(area), 0.35f, 0.9f);
                for (int l = 0; l < 3; ++l) w.col[l] = e.col[l] * k;
                g_emitters.push_back(w);
            }
        }
    }
    // the tfrags: the near image's plan run by its CPU form with an identity view (the records' world x 1024 positions; every
    // morph at its own end: the constants' fy 1, fx 0), its triangles' texels by the texture switches' CLAMP_1 (the level load's index)
    static const int kinds = std::getenv("RAC1_AUTO_LIGHTS_KINDS") != nullptr ? std::atoi(std::getenv("RAC1_AUTO_LIGHTS_KINDS")) : 3;  // (testing: 1 ties, 2 tfrags)
    if ((kinds & 1) == 0) g_emitters.clear();
    const size_t tie_emitters = g_emitters.size();
    if (g_tlevel.ok && (kinds & 2) != 0) {
        float view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        uint32_t consts[4 * 14] = {};
        const float one = 1.0f;
        uint32_t one_u;
        std::memcpy(&one_u, &one, 4);
        for (uint32_t c = 0; c < 14; ++c) consts[4 * c + 1] = one_u;
        consts[0] = one_u;  // (0x290 x: the 1 / w numerator: STQ as sent)
        std::map<std::tuple<int, int, int>, Cell> cells;
        // per tfrag texture: its whole area, its glowing area and colour, its cells - a texture glowing over much of its
        // surface in a warm colour is a material (Rilgar's, Eudora's, Orxon's ground), as for the ties' classes
        struct TexAcc {
            double total = 0.0, lit = 0.0, c[3] = {0, 0, 0};
            std::map<std::tuple<int, int, int>, Cell> cells;
        };
        std::map<int, TexAcc> tex_acc;
        std::vector<TfragCpuVertex> out;
        size_t dbg[6] = {};
        for (const TfragEntryGpu& e : g_tlevel.entries) {
            ++dbg[0];
            if (!e.ok) continue;
            const TfragPlan* p = e.plan(2, 0x00e, false);
            if (p == nullptr) p = e.plan(2, 0x010, false);
            if (p == nullptr) p = e.plan(2, 0x014, false);
            if (p == nullptr || !tfrag_exec_cpu(e, *p, 2, nullptr, view, consts, out)) continue;
            ++dbg[1];
            const std::vector<uint8_t>& img = e.image[2];
            std::vector<int> run_tex(p->runs.size(), -1);  // (a run per texture switch, in their order)
            size_t run = 0;
            for (const TfragEvent& ev : p->events) {
                if (ev.kind == 0) {
                    int cur = -1;
                    for (uint32_t r = 0; r < 5; ++r) {
                        const size_t o = 16u * (ev.at + r);
                        if (o + 16 > img.size()) break;
                        uint64_t data, reg;
                        std::memcpy(&data, img.data() + o, 8);
                        std::memcpy(&reg, img.data() + o + 8, 8);
                        if ((reg & 0xff) == 0x08) cur = static_cast<int>(data >> 24);  // (CLAMP_1's MINV: the texture's index)
                    }
                    if (run < run_tex.size()) run_tex[run] = cur;
                    ++run;
                }
            }
            for (size_t j = 2; j < out.size() && j < p->verts.size(); ++j) {
                if (p->verts[j].pos < 2) continue;
                ++dbg[2];
                const int ti = p->verts[j].run < run_tex.size() ? run_tex[p->verts[j].run] : -1;
                if (ti < 0 || static_cast<size_t>(ti) >= L.tex[0].size()) continue;
                ++dbg[3];
                const mrp::Texture& tx = L.tex[0][static_cast<size_t>(ti)];
                if (tx.width == 0 || tx.mips.empty() || tx.mips[0].size() < static_cast<size_t>(tx.width) * tx.height) continue;
                float s[3], tt[3], q[3][3];
                for (int k = 0; k < 3; ++k) {
                    const TfragCpuVertex& v = out[j - 2 + static_cast<size_t>(k)];
                    float fs, ft;
                    std::memcpy(&fs, &v.stq[0], 4);
                    std::memcpy(&ft, &v.stq[1], 4);
                    const float w = v.hp[3] != 0.0f ? v.hp[3] : 1.0f;
                    s[k] = fs * w;
                    tt[k] = ft * w;
                    for (int l = 0; l < 3; ++l) q[k][l] = v.hp[static_cast<size_t>(l)] / 1024.0f;
                }
                static const float bw[7][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {.5f, .5f, 0}, {0, .5f, .5f}, {.5f, 0, .5f}, {1 / 3.f, 1 / 3.f, 1 / 3.f}};
                int hits = 0;
                double cr = 0, cg = 0, cb = 0;
                for (const auto& b : bw) {
                    const float us = b[0] * s[0] + b[1] * s[1] + b[2] * s[2], ut = b[0] * tt[0] + b[1] * tt[1] + b[2] * tt[2];
                    const float fs = us - std::floor(us), ft = ut - std::floor(ut);
                    const uint32_t x = std::min(tx.width - 1, static_cast<uint32_t>(fs * static_cast<float>(tx.width)));
                    const uint32_t y = std::min(tx.height - 1, static_cast<uint32_t>(ft * static_cast<float>(tx.height)));
                    const uint32_t c = tx.palette[csm1(tx.mips[0][static_cast<size_t>(y) * tx.width + x])];
                    if (!emissive_rgba(c)) continue;
                    ++hits;
                    cr += (c & 0xff) / 255.0;
                    cg += ((c >> 8) & 0xff) / 255.0;
                    cb += ((c >> 16) & 0xff) / 255.0;
                }
                if (hits > 0) ++dbg[4];
                const double e1[3] = {q[1][0] - q[0][0], q[1][1] - q[0][1], q[1][2] - q[0][2]}, e2[3] = {q[2][0] - q[0][0], q[2][1] - q[0][1], q[2][2] - q[0][2]};
                const double cx = e1[1] * e2[2] - e1[2] * e2[1], cy = e1[2] * e2[0] - e1[0] * e2[2], cz = e1[0] * e2[1] - e1[1] * e2[0];
                const double len = std::sqrt(cx * cx + cy * cy + cz * cz);
                TexAcc& ta = tex_acc[ti];
                if (std::isfinite(len) && len * 0.5 <= 400.0) ta.total += 0.5 * len;
                static const int tf_min_hits = std::getenv("RAC1_TF_HITS") != nullptr ? std::atoi(std::getenv("RAC1_TF_HITS")) : 3;  // (testing)
                if (hits < tf_min_hits) continue;
                ++dbg[5];
                const double area = 0.5 * len * hits / 7.0;
                if (!(area > 0.0) || area > 64.0) continue;  // (a degenerate or large terrain triangle is ground/wall, not a lamp strip)
                ta.lit += area;
                ta.c[0] += cr / hits * area;
                ta.c[1] += cg / hits * area;
                ta.c[2] += cb / hits * area;
                // (the light a little off its surface, on the upper side: in the surface's plane it lights nothing of it)
                const double sg = cz < 0.0 ? -1.0 : 1.0;
                double ctr[3];
                for (int l = 0; l < 3; ++l) ctr[l] = (q[0][l] + q[1][l] + q[2][l]) / 3.0 + 1.2 * sg * (l == 0 ? cx : l == 1 ? cy : cz) / len;
                Cell& cell = ta.cells[{static_cast<int>(std::floor(ctr[0] / 4.0)), static_cast<int>(std::floor(ctr[1] / 4.0)), static_cast<int>(std::floor(ctr[2] / 4.0))}];
                cell.w += area;
                for (int l = 0; l < 3; ++l) cell.p[l] += ctr[l] * area;
                cell.c[0] += cr / hits * area;
                cell.c[1] += cg / hits * area;
                cell.c[2] += cb / hits * area;
            }
        }
        for (auto& [ti, ta] : tex_acc) {  // (the textures not a warm material or solid terrain surface: their cells)
            if (!(ta.lit > 0.0)) continue;
            const double share = ta.total > 0.0 ? ta.lit / ta.total : 1.0;
            const float hue = hue_of(ta.c[0] / ta.lit, ta.c[1] / ta.lit, ta.c[2] / ta.lit);
            const bool cool = hue >= 165.0f && hue <= 260.0f;
            const bool material = (!cool && (share > 0.12 || ta.lit > 70.0 || ta.total > 350.0)) || (cool && (share > 0.75 || ta.lit > 500.0));
            if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr)
                std::printf("[lights] tfrag texture %d: glowing %.1f of %.1f units^2 (%.0f %%), hue %.0f%s\n", ti, ta.lit, ta.total, 100.0 * share, hue, material ? ": a material" : "");
            if (material) continue;
            for (const auto& [key, c] : ta.cells) {
                Cell& cell = cells[key];
                cell.w += c.w;
                for (int l = 0; l < 3; ++l) {
                    cell.p[l] += c.p[l];
                    cell.c[l] += c.c[l];
                }
            }
        }
        if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr) std::printf("[lights] tfrags: entries %zu run %zu tris %zu textured %zu some %zu emissive %zu cells %zu\n", dbg[0], dbg[1], dbg[2], dbg[3], dbg[4], dbg[5], cells.size());
        static const float gain = std::getenv("RAC1_AUTO_LIGHTS_GAIN") != nullptr ? static_cast<float>(std::atof(std::getenv("RAC1_AUTO_LIGHTS_GAIN"))) : 1.0f;  // (testing)
        for (const auto& [key, cell] : cells) {
            if (cell.w < 0.10) continue;  // (a speck)
            Emitter w;
            for (int l = 0; l < 3; ++l) w.pos[l] = static_cast<float>(cell.p[l] / cell.w);
            const float area = static_cast<float>(cell.w);
            w.r = std::clamp(2.0f + 3.0f * std::sqrt(area), 2.0f, 7.0f);
            const float k = gain * std::clamp(0.35f + 0.25f * std::sqrt(area), 0.35f, 0.9f);
            for (int l = 0; l < 3; ++l) w.col[l] = static_cast<float>(cell.c[l] / cell.w) * k;
            g_emitters.push_back(w);
            if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr)
                std::printf("[lights] tfrag emitter at %.1f %.1f %.1f area %.2f colour %.2f %.2f %.2f\n", w.pos[0], w.pos[1], w.pos[2], area, cell.c[0] / cell.w, cell.c[1] / cell.w,
                            cell.c[2] / cell.w);
        }
    }
    // Merge only truly adjacent emitters of the same fixture (cap cell_size at 4.5 units so separate lamps are never averaged into mid-air)
    const size_t before_merge = g_emitters.size();
    constexpr int kDense = 88;
    const std::vector<Emitter> all = g_emitters;
    float cell_size = 3.0f;
    int dense = 0;
    for (;;) {
        struct Acc {
            double w = 0, p[3] = {}, c[3] = {};
            float r = 0;
        };
        std::map<std::tuple<int, int, int>, Acc> m;
        for (const Emitter& e : all) {
            const double w = e.col[0] + e.col[1] + e.col[2] + 1e-6;
            Acc& a = m[{static_cast<int>(std::floor(e.pos[0] / cell_size)), static_cast<int>(std::floor(e.pos[1] / cell_size)), static_cast<int>(std::floor(e.pos[2] / cell_size))}];
            a.w += w;
            for (int l = 0; l < 3; ++l) {
                a.p[l] += e.pos[l] * w;
                a.c[l] += e.col[l];
            }
            a.r = std::max(a.r, e.r);
        }
        g_emitters.clear();
        for (const auto& [key, a] : m) {
            Emitter e;
            const double mx = std::max({a.c[0], a.c[1], a.c[2]}), sc = mx > 1.0 ? 1.0 / mx : 1.0;  // (merged, at most a bright lamp's)
            for (int l = 0; l < 3; ++l) {
                e.pos[l] = static_cast<float>(a.p[l] / a.w);
                e.col[l] = static_cast<float>(a.c[l] * sc);
            }
            e.r = std::min(a.r + 0.35f * cell_size, 7.5f);
            g_emitters.push_back(e);
        }
        dense = 0;
        for (const Emitter& e : g_emitters) {
            int n = 0;
            for (const Emitter& o : g_emitters) {
                const float dx = o.pos[0] - e.pos[0], dy = o.pos[1] - e.pos[1], dz = o.pos[2] - e.pos[2];
                if (dx * dx + dy * dy + dz * dz < 60.0f * 60.0f) ++n;
            }
            dense = std::max(dense, n);
        }
        if (dense <= kDense || cell_size >= 4.5f) break;
        cell_size = 4.5f;
    }
    if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr) std::printf("[lights] merged in cells of %.1f units: at most %d lights within 60 units of one\n", cell_size, dense);
    std::printf("[lights] level %d: %zu automatic emitters, %zu before merging (%zu from %zu tie classes, %zu from the tfrags; %.0f ms)\n", L.id, g_emitters.size(), before_merge, tie_emitters, classes,
                before_merge - tie_emitters, now_ms() - t0);
}

bool g_cast_cascades = false;
int g_colours_level = -1;  // the level whose resident / tfrag colours were taken at least once (the colours held before: none) // this frame's casters outside the view made for the cascades too
// orig: (host) the casters' frame built with the static frame (its EE reads among the scene's captured inputs)
void shadow_static_build() {
    ShadowCull c;
    c.on = g_static_on && g_slevel.ok && g_mrp_world_projection == 0 && mrp_lighting_cull_params(c.sun, c.splits, &c.back, &c.tan_x, &c.tan_y);
    mrp_lighting_point_casters(c.pts);
    if (c.on && !mrp_lighting_casters_wanted()) {  // (no cascade's static cache drawn next: the game lights' shadows' alone, if any)
        bool pts = false;
        for (const auto& p : c.pts) pts = pts || p[3] > 0.0f;
        c.on = pts;
        c.cascades = false;
    }
    g_cast_cascades = c.on && c.cascades;
    c.grow = mrp_lighting_caster_grow();
    static const bool vol_game = std::getenv("RAC1_SHADOW_VOL_GAME") != nullptr;  // (testing: the passes' own view for the volume)
    if (vol_game) c.tan_x = c.tan_y = 0.0f;
    static_shadow_build(g_slevel, g_rcols, g_shframe, c);
    static const int kinds = std::getenv("RAC1_SHADOW_CAST_KINDS") != nullptr ? std::atoi(std::getenv("RAC1_SHADOW_CAST_KINDS")) : 7;  // (testing: 4 tfrags)
    if (c.on && g_tlevel.ok && (kinds & 4) != 0) tfrag_shadow_decide(g_tlevel, tfrag_snapshot(), caster_volume(c), g_tshadow, g_tshadow_views);
    else {
        g_tshadow.clear();
        g_tshadow_views.clear();
    }
}
SceneInput g_input;  // (the frame's EE inputs of the scene, captured on the game's thread)
struct SceneWorker {
    std::mutex m;
    std::condition_variable cv;
    std::thread th;
    bool job = false, done = false, quit = false;
    bool capture = false, captured = true;  // (the job captures its inputs first; `captured` once they are)
    SceneAddrs addrs;
    ColourSource colours;
    uint32_t chain_start = 0, chain_end = 0;
    bool reset = false, kicks = false, built = false;
    bool build = false;  // (the job runs build_triangles after the scene)
    std::exception_ptr error;
    double ms = 0;
    void run() {
        std::unique_lock<std::mutex> lk(m);
        for (;;) {
            cv.wait(lk, [this] { return job || quit; });
            if (quit) return;
            lk.unlock();
            const double t0 = now_ms();
            const rac1::prof::Job prof_job(rac1::prof::kWorkerScene);  // (the debug menu's profiler: the job's time)
            try {
                if (capture) {
                    // (the game's point lights per pixel instead; never before the level's colours were taken once)
                    const bool freeze = mrp_lighting_points_freeze() && g_colours_level == g_level.id;
                    if (g_static_on) {  // (the colours alone kept while a light is in use: the instances are this frame's)
                        if (!freeze) resident_colours_update(g_rcols, colours);
                        static_frame_build(g_slevel, g_rcols, g_sframe, g_static_check);
                    }
                    shadow_static_build();
                    if (g_input.tfrag_level != nullptr && !freeze) tfrag_colours_update(g_tcols, g_tlevel);
                    if (!freeze) g_colours_level = g_level.id;
                    if (g_input.tfrag_host != nullptr) tfrag_decide(g_tlevel, tfrag_snapshot(), g_tfrag_check, g_tdecide);
                    scene_capture(chain_start, chain_end, g_input, addrs);
                    {
                        std::lock_guard<std::mutex> lg(m);
                        captured = true;
                    }
                    cv.notify_all();
                }
                built = scene_build(*g_level.idx, g_input, g_frame.scene, reset, kicks);
                g_frame.built_on_worker = false;
                if (built && build) {  // the frame's triangles too, its new textures' images left to the host thread
                    const double tb = now_ms();
                    g_tex_defer = true;
                    build_triangles(g_frame);
                    g_tex_defer = false;
                    g_frame.ms_build = now_ms() - tb;
                    g_frame.built_on_worker = true;
                }
            } catch (...) {
                g_tex_defer = false;
                error = std::current_exception();
                built = false;
            }
            ms = now_ms() - t0;
            lk.lock();
            job = false;
            done = true;
            captured = true;
            cv.notify_all();
        }
    }
    void start(uint32_t cs, uint32_t ce, bool rs, bool ks, bool cap, const SceneAddrs& ad, const ColourSource& col, bool bt) {
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this] { return !job; });  // (a job not waited for: its frame's outputs are about to be written again)
            if (!th.joinable()) th = std::thread([this] {
                rac1::prof::thread_name("MRP scene worker");
                run();
            });
            colours = col;
            capture = cap;
            captured = !cap;
            addrs = ad;
            chain_start = cs;
            chain_end = ce;
            reset = rs;
            kicks = ks;
            build = bt;
            done = false;
            error = nullptr;
            job = true;
        }
        cv.notify_all();
    }
    void wait_captured() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this] { return captured; });
    }
    bool wait() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this] { return done; });
        if (error) std::rethrow_exception(error);
        return built;
    }
    ~SceneWorker() {
        {
            std::lock_guard<std::mutex> lk(m);
            quit = true;
        }
        cv.notify_all();
        if (th.joinable()) th.join();
    }
};
SceneWorker g_worker;
// (RAC1_MRPW_THREAD=0: the scene built on the host thread, at mrp_world_begin_frame_async as before; testing modes too)
bool scene_threaded() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_MRPW_THREAD");
        return (e == nullptr || std::atoi(e) != 0) && !g_env_check && !g_env_diff;
    }();
    return on;
}
// (RAC1_MRPW_BUILD_THREAD=0: the frame's triangles built on the host thread when it takes the frame, as before; the testing
// modes too - their checks compare the build with the faithful path's on the host thread)
bool build_threaded() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_MRPW_BUILD_THREAD");
        return scene_threaded() && (e == nullptr || std::atoi(e) != 0) && !g_tfrag_verify && std::getenv("RAC1_MRPW_CPU") == nullptr;
    }();
    return on;
}
double g_t_begin = 0;
}  // namespace

// orig: (host) see mrp_world_renderer.h
void mrp_world_begin_frame_async(bool active, uint32_t chain_start, uint32_t chain_end) {
    Frame& F = g_frame;
    F.active = false;
    F.skips.clear();
    F.skips_ready = false;
    F.pending = false;
    if (!g_world_mrp || !active) return;
    if (!init_gl()) return;
    const int id = world_level_id();
    // (a level without its asset is not read again every frame: tried, and no world)
    if (g_level.id != id || !g_level.tried || (g_level.world != nullptr && (vk::is_active() ? g_level.vk_tvbo.buffer == VK_NULL_HANDLE : g_level.vao == 0))) {
        load_level(id);
        g_rcols.all_dirty = true;
    }
    if (!g_level.loaded) return;
    g_t_begin = now_ms();
    {  // build_triangles' inputs from the host's state (Frame::BuildInputs)
        mrp_world_view_proj(F.in.vp);
        F.in.widen = mrp_lod_frame_widen();
        F.in.projection = g_mrp_world_projection;
        F.in.modern_shadows = g_modern_shadows;
        F.in.sky = g_render_sky;
        F.in.tfrag = g_render_tfrag;
        F.in.tie = g_render_tie;
        F.in.shrub = g_render_shrub;
    }
    {  // the frame's draw environment's XYOFFSET_1 (0x13cf10, 11 qwords)
        F.env_xyoff = 0;
        for (uint32_t q = 0; q < 0xb; ++q) {
            const uint32_t a = 0x0013cf10 + q * 16;
            if (lbu(a + 8) == 0x18) F.env_xyoff = static_cast<uint64_t>(lwu(a)) | (static_cast<uint64_t>(lwu(a + 4)) << 32);
        }
    }
    {  // the GS registers before the frame's chain (build_triangles): as the last chain read left them
        for (uint32_t r = 0; r < 128; ++r) F.gs_regs[r] = mrp_chain_gs_reg(r);
    }
    // the passes' ranges the replica skips: scene_build builds a scene exactly when there are passes and a chain
    static const bool sky_noskip = std::getenv("RAC1_MRPW_SKY_NOSKIP") != nullptr;  // (testing: the replica runs the sky's range too, drawn twice)
    if (!scene_passes().empty() && chain_start != 0) {
        for (const PassRange& r : scene_passes())
            if (r.pass != 3 || !sky_noskip) F.skips.push_back({r.first, r.end - 16});
        F.skips_ready = true;
    }
    const bool kicks = g_env_check || std::getenv("RAC1_MRPW_CPU") != nullptr;
    F.pending = true;
    g_input.colours = g_static_on ? &g_rcols : nullptr;
    g_input.stat = g_static_on ? &g_sframe : nullptr;
    g_input.stat_level = g_static_on ? &g_slevel : nullptr;
    g_input.stat_check = g_static_check;
    {  // the tfrag GPU path: the world projection, not the testing modes
        const bool on = tfrag_gpu_enabled() && g_tlevel.ok && g_mrp_world_projection == 0 && !g_env_check && !g_env_diff && tfrag_gpu_init();
        g_input.tfrag_level = on ? &g_tlevel : nullptr;
        g_input.tfrag_colours = on ? &g_tcols : nullptr;
        g_input.tfrag_kicks = on && g_tfrag_verify;
        const TfragSnapshot& S = tfrag_snapshot();  // (the decisions: made in the capture step, from the pass's snapshot)
        g_input.tfrag_host = on && S.valid && (g_tfrag_check || (S.handle && mrp_world_tfrag_lod())) ? &g_tdecide : nullptr;
        g_input.tfrag_check = g_tfrag_check;
        g_input.tfrag_shadow = g_modern_shadows != 0 && g_input.tfrag_level != nullptr ? &g_tshadow : nullptr;
        g_tfrag_shadow_on = g_input.tfrag_shadow != nullptr;
        g_input.tfrag_shadow_views = &g_tshadow_views;
    }
    if (scene_threaded()) {  // (the capture on the worker too: the host must call mrp_world_wait_captured before the game's state changes)
        g_worker.start(chain_start, chain_end, g_level.reset_walker, kicks, true, scene_addrs(), colour_source(), build_threaded());
    } else {
        const bool freeze = mrp_lighting_points_freeze() && g_colours_level == g_level.id;
        if (g_static_on) {  // (the colours alone kept while a light is in use: the instances are this frame's)
            if (!freeze) resident_colours_update(g_rcols, colour_source());
            static_frame_build(g_slevel, g_rcols, g_sframe, g_static_check);
        }
        shadow_static_build();
        if (g_input.tfrag_level != nullptr && !freeze) tfrag_colours_update(g_tcols, g_tlevel);
        if (!freeze) g_colours_level = g_level.id;
        if (g_input.tfrag_host != nullptr) tfrag_decide(g_tlevel, tfrag_snapshot(), g_tfrag_check, g_tdecide);
        scene_capture(chain_start, chain_end, g_input);
        g_worker.built = scene_build(*g_level.idx, g_input, F.scene, g_level.reset_walker, kicks);
        g_worker.ms = now_ms() - g_t_begin;
    }
    g_level.reset_walker = false;
}

// orig: (host) see mrp_world_renderer.h
bool mrp_world_pending() { return g_frame.pending; }

// orig: (host) see mrp_world_renderer.h
void mrp_world_wait_captured() {
    RAC1_PROF_ZONE(kSceneCaptureWait);
    if (g_frame.pending && scene_threaded()) g_worker.wait_captured();
}

// orig: (host) see mrp_world_renderer.h
void mrp_world_drop_frame() {
    Frame& F = g_frame;
    if (!F.pending) return;
    F.pending = false;
    if (scene_threaded()) {
        try {
            g_worker.wait();
        } catch (...) {
        }
    }
    create_new_buckets(g_level);  // (the textures its build found: their buckets' indices are taken)
    F.skips.clear();
    F.skips_ready = false;
    F.active = false;
}

// orig: (host) see mrp_world_renderer.h
void mrp_world_finish_frame() {
    Frame& F = g_frame;
    if (!F.pending) return;
    F.pending = false;
    F.uploaded = false;
    const int prof_wait = rac1::prof::begin(rac1::prof::kSceneWait);
    bool built = false;
    try {
        built = scene_threaded() ? g_worker.wait() : g_worker.built;
    } catch (...) {
        create_new_buckets(g_level);
        throw;
    }
    rac1::prof::end(prof_wait);
    create_new_buckets(g_level);  // (the textures the worker's build found, their images made here)
    const double t1 = now_ms();
    if (!built) {
        F.skips.clear();
        F.skips_ready = false;
        return;
    }
    for (int i = 0; i < 6; ++i) {
        g_st.draws[i] += F.scene.draws[i];
        g_st.unknown[i] += F.scene.unknown[i];
    }
    g_st.setups += F.scene.setups;
    if (F.scene.has_sky) {
        ++g_st.sky_frames;
        g_st.sky_packets += F.scene.sky_packets;
        g_st.sky_kicks += F.scene.sky_kicks;
        g_st.ms_sky_scene += F.scene.ms_sky;
    }
    g_st.gif_packets += F.scene.gif_packets;
    g_st.unknown_program += F.scene.unknown_program;
    g_st.col_resident += F.scene.resident;
    g_st.col_copied += F.scene.copied;
    g_st.col_verified += F.scene.verified;
    g_st.col_bad += F.scene.verify_bad;
    for (int k = 0; k < 2; ++k) {
        g_st.stat_draws[k] += F.scene.stat_draws[k];
        g_st.chk_walker[k] += F.scene.chk_walker[k];
        g_st.chk_found[k] += F.scene.chk_found[k];
        g_st.chk_missing[k] += F.scene.chk_missing[k];
        g_st.chk_extra[k] += F.scene.chk_extra[k];
    }
    g_st.tfrag_cpu_draws += F.scene.tfrag_cpu;
    g_st.tfrag_checked += F.scene.tfrag_checked;
    g_st.tfrag_bad += F.scene.tfrag_bad;
    g_st.tfrag_host += F.scene.tfrag_host;
    g_st.tchk_walker += F.scene.tchk_walker;
    g_st.tchk_found += F.scene.tchk_found;
    g_st.tchk_missing += F.scene.tchk_missing;
    g_st.tchk_extra += F.scene.tchk_extra;
    g_st.tchk_code += F.scene.tchk_code;
    g_st.stat_variants += F.scene.stat_variants;
    g_st.chk_gif_frames += F.scene.chk_gif_frames;
    g_st.chk_gif_bad += F.scene.chk_gif_bad;
    g_st.chk_gif_quads += F.scene.chk_gif_quads;
    g_st.chk_morph_bad += F.scene.chk_morph_bad;
    g_st.chk_fog_bad += F.scene.chk_fog_bad;
    g_st.chk_alpha_bad += F.scene.chk_alpha_bad;
    g_st.chk_rows_bad += F.scene.chk_rows_bad;
    g_st.chk_rows_maxrel = std::max(g_st.chk_rows_maxrel, F.scene.chk_rows_maxrel);
    for (const std::string& n : F.scene.chk_notes)
        if (g_st.chk_notes.size() < 30) g_st.chk_notes.push_back(n);
    ++g_st.scene_frames;
    if (!F.built_on_worker) {
        build_triangles(F);
        F.ms_build = now_ms() - t1;
    }
    g_st.ms_scene += g_worker.ms - (F.built_on_worker ? F.ms_build : 0.0);  // (the scene's own time, on the worker or not)
    g_st.ms_wait += t1 - g_t_begin;
    g_st.ms_build += F.ms_build;
    F.active = true;
}

// orig: (host) see mrp_world_renderer.h
void mrp_world_begin_frame(bool active, uint32_t chain_start, uint32_t chain_end) {
    mrp_world_begin_frame_async(active, chain_start, chain_end);
    mrp_world_finish_frame();
}

int g_last_vp[4] = {};  // (mrp_world_last_viewport)
// orig: (host) see mrp_world_renderer.h
bool mrp_world_last_viewport(int out[4]) {
    std::memcpy(out, g_last_vp, sizeof g_last_vp);
    return g_last_vp[2] > 0 && g_last_vp[3] > 0;
}

// orig: (host) see mrp_world_renderer.h
void mrp_world_draw(int win_w, int win_h, const gs::Viewport* vp) {
    (void)win_w;
    (void)win_h;
    if (!g_frame.active) return;
    if (g_env_prof_finish && !vk::is_active()) glFinish();  // (the GPU work before it, e.g. the sky, not counted)
    const double t0 = now_ms();
    if (vp != nullptr) {
        if (vk::is_active()) vk::set_viewport(vp->x, vp->y, vp->w, vp->h);
        else glViewport(vp->x, vp->y, vp->w, vp->h);
        g_last_vp[0] = vp->x;
        g_last_vp[1] = vp->y;
        g_last_vp[2] = vp->w;
        g_last_vp[3] = vp->h;
    }
    draw_frame(vp);
    if (g_env_prof) {
        g_st.ms_submit += now_ms() - t0;
        if (g_env_prof_finish && !vk::is_active()) glFinish();
        g_st.ms_draw += now_ms() - t0;
        if (++g_st.prof_frames >= 120) {
            const double n = static_cast<double>(g_st.prof_frames);
            std::printf("[prof] mrp_world ms/frame: scene %.3f (from its start to its end on the host thread %.3f) triangles %.3f draw %.3f (its GL calls %.3f) | %zu packets, the CPU path's %zu vertices %zu triangles, the GPU path's "
                        "%zu instances %zu draw commands; %zu batches\n",
                        g_st.ms_scene / n, g_st.ms_wait / n, g_st.ms_build / n, g_st.ms_draw / n, g_st.ms_submit / n, g_frame.scene.packets.size(), g_frame.v.size(), g_frame.idx.size() / 3,
                        g_frame.insts.size(), g_frame.draws.size(), g_frame.batches.size());
            {  // the tfrag GPU path (last frame)
                size_t tb = 0;
                for (const Batch& b : g_frame.batches) tb += b.tfrag ? 1 : 0;
                std::printf("[prof] mrp_tfrag (last frame): %zu draws (%u host-made), %u vertices, %u triangle slots, %zu runs, %zu batches\n", g_frame.tf.draws.size(),
                            g_frame.scene.tfrag_host, g_frame.tf.nverts, g_frame.tf.nidx / 3, g_frame.tf.runs.size() / 4, tb);
            }
            std::printf("[prof] mrp_world scene (last frame) set-ups / draws ms: tfrag A %.3f / %.3f, tfrag B %.3f / %.3f, tie A %.3f / %.3f, tie B %.3f / %.3f, shrub A %.3f / %.3f, shrub B %.3f / %.3f\n",
                        g_frame.scene.ms_setup[0], g_frame.scene.ms_draw[0], g_frame.scene.ms_setup[1], g_frame.scene.ms_draw[1], g_frame.scene.ms_setup[2], g_frame.scene.ms_draw[2],
                        g_frame.scene.ms_setup[3], g_frame.scene.ms_draw[3], g_frame.scene.ms_setup[4], g_frame.scene.ms_draw[4], g_frame.scene.ms_setup[5], g_frame.scene.ms_draw[5]);
            std::printf("[prof] mrp_sky ms/frame: scene (its chain range: the GIF streams, the sprites' VU1 program) %.3f, its GPU draw %.3f | %u packets (%u VU1 kicks), %zu "
                        "register writes\n",
                        g_st.ms_sky_scene / n, g_st.ms_sky / n, g_frame.scene.sky_packets, g_frame.scene.sky_kicks, g_frame.scene.sky.size());
            g_st.ms_sky_scene = g_st.ms_sky = 0;
            g_st.ms_scene = g_st.ms_build = g_st.ms_draw = g_st.ms_submit = g_st.ms_wait = 0;
            g_st.prof_frames = 0;
        }
    }
    ++g_st.frames;
}

// orig: (host) see mrp_lighting.h
void mrp_world_draw_casters(const float* light_vp, int view) {
    if (!g_frame.active) return;
    draw_frame(nullptr, false, light_vp, view);
}

// orig: (host) see mrp_lighting.h
std::vector<EmitterClass> mrp_world_emitter_classes() { return g_emitter_classes; }
// orig: (host) see mrp_lighting.h
void mrp_world_emitters_rebuild() { g_emitters_level = -2; }

namespace {
// orig: (host) the distance from p to the triangle (a, b, c) (the closest point's, Ericson's regions)
float point_triangle_dist(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return glm::length(p - a);
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return glm::length(p - b);
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return glm::length(p - (a + ab * (d1 / (d1 - d3))));
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return glm::length(p - c);
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return glm::length(p - (a + ac * (d2 / (d2 - d6))));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) return glm::length(p - (b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)))));
    const float den = 1.0f / (va + vb + vc);
    return glm::length(p - (a + ab * (vb * den) + ac * (vc * den)));
}
// the level's tfrag triangles in world units with their texture (the near image's plan run on the CPU, as the automatic emitters'),
// made at the first pick of a level
struct TfragTris {
    int level = -2;
    std::vector<glm::vec3> v;  // 3 per triangle
    std::vector<int> tex;
};
TfragTris g_tf_tris;
void tfrag_tris_build() {
    const Level& L = g_level;
    g_tf_tris.level = L.id;
    g_tf_tris.v.clear();
    g_tf_tris.tex.clear();
    if (!g_tlevel.ok) return;
    float view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    uint32_t consts[4 * 14] = {};
    const float one = 1.0f;
    uint32_t one_u;
    std::memcpy(&one_u, &one, 4);
    for (uint32_t c = 0; c < 14; ++c) consts[4 * c + 1] = one_u;
    consts[0] = one_u;
    std::vector<TfragCpuVertex> out;
    for (const TfragEntryGpu& e : g_tlevel.entries) {
        if (!e.ok) continue;
        const TfragPlan* p = e.plan(2, 0x00e, false);
        if (p == nullptr) p = e.plan(2, 0x010, false);
        if (p == nullptr) p = e.plan(2, 0x014, false);
        if (p == nullptr || !tfrag_exec_cpu(e, *p, 2, nullptr, view, consts, out)) continue;
        const std::vector<uint8_t>& img = e.image[2];
        std::vector<int> run_tex(p->runs.size(), -1);
        size_t run = 0;
        for (const TfragEvent& ev : p->events) {
            if (ev.kind != 0) continue;
            int cur = -1;
            for (uint32_t r = 0; r < 5; ++r) {
                const size_t o = 16u * (ev.at + r);
                if (o + 16 > img.size()) break;
                uint64_t data, reg;
                std::memcpy(&data, img.data() + o, 8);
                std::memcpy(&reg, img.data() + o + 8, 8);
                if ((reg & 0xff) == 0x08) cur = static_cast<int>(data >> 24);  // (CLAMP_1's MINV: the texture's index)
            }
            if (run < run_tex.size()) run_tex[run] = cur;
            ++run;
        }
        for (size_t j = 2; j < out.size() && j < p->verts.size(); ++j) {
            if (p->verts[j].pos < 2) continue;
            const int ti = p->verts[j].run < run_tex.size() ? run_tex[p->verts[j].run] : -1;
            if (ti < 0) continue;
            for (int k = 0; k < 3; ++k) {
                const TfragCpuVertex& v = out[j - 2 + static_cast<size_t>(k)];
                g_tf_tris.v.push_back(glm::vec3(v.hp[0], v.hp[1], v.hp[2]) / 1024.0f);
            }
            g_tf_tris.tex.push_back(ti);
        }
    }
}
// orig: (host) a tie / shrub instance's frame: world = pos + M x model point (the instance's rows x its scale / 1024)
struct InstFrame {
    glm::vec3 pos;
    glm::mat3 m;
    glm::vec4 sphere;
};
}  // namespace

// orig: (host) see mrp_lighting.h
int mrp_world_class_texture(int kind, int o_class, int tex, int* ntex) { return class_texture(g_level, kind, o_class, tex, ntex); }

// orig: (host) see mrp_lighting.h
SurfacePick mrp_world_surface_at(const float pp[3]) {
    SurfacePick r;
    const Level& L = g_level;
    if (!L.loaded || L.world == nullptr || !g_slevel.ok) return r;
    const glm::vec3 P(pp[0], pp[1], pp[2]);
    float best = 0.5f;  // (units: a triangle farther than this is not the one under the crosshair)
    auto consider = [&](int kind, int o_class, int tex, int texture, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        const float d = point_triangle_dist(P, a, b, c);
        if (d >= best) return;
        best = d;
        r.kind = kind;
        r.o_class = o_class;
        r.tex = tex;
        r.texture = texture;
        r.dist = d;
        const glm::vec3 n = glm::cross(b - a, c - a);
        const float l = glm::length(n);
        if (l > 1e-12f)
            for (int k = 0; k < 3; ++k) r.normal[k] = n[k] / l;
    };
    std::unordered_map<const TiePacket*, const TieClass*> cls_of;
    for (const TieClass& c : L.world->ties)
        for (const TiePacket& p : c.packets) cls_of[&p] = &c;
    std::vector<InstFrame> near;
    for (const StaticTieClass& sc : g_slevel.ties) {
        if (sc.lod[0].empty() || sc.lod[0].front().tie == nullptr) continue;
        const auto ci = cls_of.find(sc.lod[0].front().tie);
        if (ci == cls_of.end()) continue;
        const TieClass& tc = *ci->second;
        near.clear();
        const uint32_t bounds = lwu(sc.rec + 0x28), n = lhu(sc.rec + 0x26);
        for (uint32_t i = 0; i < n; ++i) {
            InstFrame f;
            for (uint32_t l = 0; l < 4; ++l) f.sphere[static_cast<int>(l)] = lwc1(bounds + 0x20 * i + 4 * l);
            if (glm::length(P - glm::vec3(f.sphere)) > f.sphere.w + 1.0f) continue;
            const uint32_t data = lwu(bounds + 0x20 * i + 0x10);
            const float w = lwc1(data + 0x3c);
            f.pos = glm::vec3(lwc1(data + 0x30), lwc1(data + 0x34), lwc1(data + 0x38));
            for (uint32_t c = 0; c < 3; ++c)
                for (uint32_t l = 0; l < 3; ++l) f.m[static_cast<int>(c)][static_cast<int>(l)] = lwc1(data + 0x10 * c + 4 * l) * w / 1024.0f;
            near.push_back(f);
        }
        if (near.empty()) continue;
        for (const StaticPacket& sp : sc.lod[0]) {
            const auto ti = L.tpl.find(sp.tie);
            if (ti == L.tpl.end()) continue;
            const Tpl& t = ti->second;
            for (const TplRun& run : t.runs) {
                if (run.slot < 0 || static_cast<size_t>(run.slot) >= sp.tie->prims.size()) continue;
                const int prim = sp.tie->prims[static_cast<size_t>(run.slot)];
                if (prim < 0 || static_cast<size_t>(prim) >= tc.textures.size()) continue;
                for (uint32_t k = 0; k + 2 < run.count; k += 3) {
                    glm::vec3 v[3];
                    for (int j = 0; j < 3; ++j) {
                        const TVert& tv = L.tverts[t.first_vertex + L.tidx[t.first_index + run.first + k + static_cast<uint32_t>(j)]];
                        v[j] = glm::vec3(tv.pos[0], tv.pos[1], tv.pos[2]);
                    }
                    for (const InstFrame& f : near)
                        consider(1, tc.o_class, prim, tc.textures[static_cast<size_t>(prim)].texture, f.pos + f.m * v[0], f.pos + f.m * v[1], f.pos + f.m * v[2]);
                }
            }
        }
    }
    const uint32_t shrub_data = lwu(OV(0x0016049cu));
    for (size_t c = 0; c < g_slevel.shrubs.size() && c < L.world->shrubs.size(); ++c) {
        const StaticShrubClass& sc = g_slevel.shrubs[c];
        const ShrubClass& cls = L.world->shrubs[c];
        if (sc.packets.empty()) continue;
        near.clear();
        const uint32_t bounds = lwu(sc.rec + 0x18), n = lhu(sc.rec + 0x16);
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t b = bounds + 0x20 * i;
            InstFrame f;
            for (uint32_t l = 0; l < 4; ++l) f.sphere[static_cast<int>(l)] = lwc1(b + 4 * l);
            if (glm::length(P - glm::vec3(f.sphere)) > f.sphere.w + 1.0f) continue;
            const uint32_t data = shrub_data + 0x40 * lhu(b + 0x18);
            const float w = lwc1(data + 0x3c);
            f.pos = glm::vec3(lwc1(data + 0x30), lwc1(data + 0x34), lwc1(data + 0x38));
            for (uint32_t cc = 0; cc < 3; ++cc)
                for (uint32_t l = 0; l < 3; ++l) f.m[static_cast<int>(cc)][static_cast<int>(l)] = lwc1(data + 0x10 * cc + 4 * l) * w / 1024.0f;
            near.push_back(f);
        }
        if (near.empty()) continue;
        for (const StaticPacket& sp : sc.packets) {
            if (sp.shrub == nullptr) continue;
            const auto ti = L.tpl.find(sp.shrub);
            if (ti == L.tpl.end()) continue;
            const Tpl& t = ti->second;
            for (const TplRun& run : t.runs) {
                if (run.slot < 0 || static_cast<size_t>(run.slot) >= sp.shrub->textures.size()) continue;
                const TexturePrim& tp = sp.shrub->textures[static_cast<size_t>(run.slot)];
                for (uint32_t k = 0; k + 2 < run.count; k += 3) {
                    glm::vec3 v[3];
                    for (int j = 0; j < 3; ++j) {
                        const TVert& tv = L.tverts[t.first_vertex + L.tidx[t.first_index + run.first + k + static_cast<uint32_t>(j)]];
                        v[j] = glm::vec3(tv.pos[0], tv.pos[1], tv.pos[2]);
                    }
                    for (const InstFrame& f : near)
                        consider(2, cls.o_class, tp.local_texture, tp.texture, f.pos + f.m * v[0], f.pos + f.m * v[1], f.pos + f.m * v[2]);
                }
            }
        }
    }
    if (g_tf_tris.level != L.id) tfrag_tris_build();
    for (size_t i = 0; i < g_tf_tris.tex.size(); ++i) {
        const glm::vec3* v = &g_tf_tris.v[3 * i];
        const glm::vec3 lo = glm::min(glm::min(v[0], v[1]), v[2]) - best, hi = glm::max(glm::max(v[0], v[1]), v[2]) + best;
        if (glm::any(glm::lessThan(P, lo)) || glm::any(glm::greaterThan(P, hi))) continue;
        consider(0, 0, g_tf_tris.tex[i], g_tf_tris.tex[i], v[0], v[1], v[2]);
    }
    if (r.kind >= 0) class_texture(L, r.kind, r.o_class, r.tex, &r.ntex);
    return r;
}

// orig: (host) testing (RAC1_LIGHTS_TRACE, the materials' survey): each tfrag / tie / shrub texture of the level with its mean colour
// and the classes that use it - lava, water, mud found by their colour, then looked at (docs/materials.md)
void mrp_world_texture_survey() {
    const Level& L = g_level;
    if (!L.loaded || L.world == nullptr) return;
    auto csm1 = [](uint32_t i) { return ((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i; };
    std::map<int, std::string> users[3];
    for (const TieClass& tc : L.world->ties)
        for (size_t i = 0; i < tc.textures.size(); ++i)
            if (tc.textures[i].texture >= 0) users[1][tc.textures[i].texture] += " tie " + std::to_string(tc.o_class) + " tex " + std::to_string(i) + ";";
    for (const ShrubClass& sc : L.world->shrubs)
        for (const ShrubPacket& p : sc.packets)
            for (const TexturePrim& t : p.textures)
                if (t.texture >= 0 && users[2][t.texture].find(" shrub " + std::to_string(sc.o_class) + " ") == std::string::npos)
                    users[2][t.texture] += " shrub " + std::to_string(sc.o_class) + " tex " + std::to_string(t.local_texture) + ";";
    for (int table = 0; table < 3; ++table)
        for (size_t n = 0; n < L.tex[table].size(); ++n) {
            const mrp::Texture& tx = L.tex[table][n];
            if (tx.width == 0 || tx.mips.empty() || tx.mips[0].size() < static_cast<size_t>(tx.width) * tx.height) continue;
            double c[3] = {0, 0, 0};
            const size_t m = static_cast<size_t>(tx.width) * tx.height;
            for (size_t q = 0; q < m; ++q) {
                const uint32_t v = tx.palette[csm1(tx.mips[0][q])];
                for (int k = 0; k < 3; ++k) c[k] += (v >> (8 * k)) & 0xff;
            }
            const std::string u = table == 0 ? std::string(" tfrag 0 tex ") + std::to_string(n) : users[table][static_cast<int>(n)].substr(0, 160);
            std::printf("[lights] texture %s %zu (%dx%d): mean %.0f %.0f %.0f:%s\n", table == 0 ? "tfrag" : table == 1 ? "tie" : "shrub", n, tx.width, tx.height, c[0] / m, c[1] / m,
                        c[2] / m, u.c_str());
        }
}

// orig: (host) see mrp_lighting.h
std::vector<TextureEntry> mrp_world_texture_list(int thumb) {
    std::vector<TextureEntry> out;
    const Level& L = g_level;
    if (!L.loaded || L.world == nullptr) return out;
    auto csm1 = [](uint32_t i) { return ((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i; };
    std::map<int, std::vector<std::pair<int, int>>> users[3];
    for (const TieClass& tc : L.world->ties)
        for (size_t i = 0; i < tc.textures.size(); ++i)
            if (tc.textures[i].texture >= 0) users[1][tc.textures[i].texture].push_back({tc.o_class, static_cast<int>(i)});
    for (const ShrubClass& sc : L.world->shrubs)
        for (const ShrubPacket& p : sc.packets)
            for (const TexturePrim& t : p.textures)
                if (t.texture >= 0) {
                    auto& u = users[2][t.texture];
                    const std::pair<int, int> e{sc.o_class, t.local_texture};
                    if (std::find(u.begin(), u.end(), e) == u.end()) u.push_back(e);
                }
    for (int table = 0; table < 3; ++table)
        for (size_t n = 0; n < L.tex[table].size(); ++n) {
            const mrp::Texture& tx = L.tex[table][n];
            if (tx.width == 0 || tx.mips.empty() || tx.mips[0].size() < static_cast<size_t>(tx.width) * tx.height) continue;
            TextureEntry e;
            e.kind = table;
            e.texture = static_cast<int>(n);
            e.w = static_cast<int>(tx.width);
            e.h = static_cast<int>(tx.height);
            if (table == 0) e.users.push_back({0, static_cast<int>(n)});  // (a tfrag texture: "tfrag 0 tex <n>")
            else if (const auto it = users[table].find(static_cast<int>(n)); it != users[table].end()) e.users = it->second;
            if (table != 0 && e.users.empty()) continue;  // (no class of the level uses it)
            e.thumb.resize(static_cast<size_t>(thumb) * thumb);
            for (int y = 0; y < thumb; ++y)
                for (int x = 0; x < thumb; ++x) {
                    const uint32_t sx = static_cast<uint32_t>(x) * tx.width / static_cast<uint32_t>(thumb), sy = static_cast<uint32_t>(y) * tx.height / static_cast<uint32_t>(thumb);
                    e.thumb[static_cast<size_t>(y) * thumb + x] = tx.palette[csm1(tx.mips[0][sy * tx.width + sx])] | 0xff000000u;
                }
            out.push_back(std::move(e));
        }
    return out;
}

// orig: (host) see mrp_lighting.h
std::vector<EmissiveCandidate> mrp_world_emissive_candidates() {
    std::vector<EmissiveCandidate> out;
    const Level& L = g_level;
    if (!L.loaded || L.world == nullptr) return out;
    auto csm1 = [](uint32_t i) { return ((i & 0x08) != 0) != ((i & 0x10) != 0) ? i ^ 0x18u : i; };
    std::map<std::pair<int, int>, bool> seen;  // (kind, level texture)
    auto test = [&](int kind, int o_class, int tex, int texture) {
        const int table = kind == 1 ? 1 : 2;
        if (texture < 0 || static_cast<size_t>(texture) >= L.tex[table].size() || !seen.insert({{kind, texture}, true}).second) return;
        const mrp::Texture& tx = L.tex[table][static_cast<size_t>(texture)];
        if (tx.width == 0 || tx.mips.empty() || tx.mips[0].size() < static_cast<size_t>(tx.width) * tx.height) return;
        size_t glow = 0, white = 0, n = static_cast<size_t>(tx.width) * tx.height;
        for (size_t q = 0; q < n; ++q) {
            const uint32_t c = tx.palette[csm1(tx.mips[0][q])];
            if (emissive_rgba(c)) ++glow;
            const int r8 = static_cast<int>(c & 0xff), g8 = static_cast<int>((c >> 8) & 0xff), b8 = static_cast<int>((c >> 16) & 0xff);
            const int mx = std::max({r8, g8, b8}), mn = std::min({r8, g8, b8});
            if (mn >= 0xb0 && mx - mn <= mx / 6) ++white;  // (near white: RAC1_LIGHTS_WHITE's test)
        }
        EmissiveCandidate c{kind, o_class, tex, texture, static_cast<float>(glow) / n, static_cast<float>(white) / n};
        if ((c.glow >= 0.02f || c.white >= 0.02f) && c.glow + c.white <= 0.7f) out.push_back(c);
    };
    for (const TieClass& tc : L.world->ties)
        for (size_t i = 0; i < tc.textures.size(); ++i) test(1, tc.o_class, static_cast<int>(i), tc.textures[i].texture);
    for (const ShrubClass& sc : L.world->shrubs)
        for (const ShrubPacket& p : sc.packets)
            for (const TexturePrim& t : p.textures) test(2, sc.o_class, t.local_texture, t.texture);
    std::sort(out.begin(), out.end(), [](const EmissiveCandidate& a, const EmissiveCandidate& b) { return a.glow + a.white > b.glow + b.white; });
    if (out.size() > 80) out.resize(80);
    return out;
}

// orig: (host) see mrp_lighting.h
const std::vector<TieSpot>& mrp_world_tie_instances() {
    static std::vector<TieSpot> list;
    static int level = -2;
    static std::mutex m;
    std::lock_guard<std::mutex> lg(m);
    if (level == g_level.id && g_level.loaded) return list;
    list.clear();
    level = g_level.id;
    const Level& L = g_level;
    if (!L.loaded || L.world == nullptr || !g_slevel.ok) return list;
    std::unordered_map<const TiePacket*, const TieClass*> cls_of;
    for (const TieClass& c : L.world->ties)
        for (const TiePacket& p : c.packets) cls_of[&p] = &c;
    for (const StaticTieClass& sc : g_slevel.ties) {  // (as the automatic emitters' instances: the level's tables)
        if (sc.lod[0].empty() || sc.lod[0].front().tie == nullptr) continue;
        const auto ci = cls_of.find(sc.lod[0].front().tie);
        if (ci == cls_of.end()) continue;
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};  // (the model's box: its LOD 0 vertices)
        for (const StaticPacket& sp : sc.lod[0]) {
            const auto ti = L.tpl.find(sp.tie);
            if (ti == L.tpl.end()) continue;
            const Tpl& t = ti->second;
            for (const TplRun& run : t.runs)
                for (uint32_t k = 0; k < run.count; ++k) {
                    const TVert& v = L.tverts[t.first_vertex + L.tidx[t.first_index + run.first + k]];
                    for (int l = 0; l < 3; ++l) {
                        lo[l] = std::min(lo[l], static_cast<float>(v.pos[l]) / 1024.0f);
                        hi[l] = std::max(hi[l], static_cast<float>(v.pos[l]) / 1024.0f);
                    }
                }
        }
        if ((lhu(sc.rec + 0x24) & 9) != 0) continue;
        const uint32_t bounds = lwu(sc.rec + 0x28), n = lhu(sc.rec + 0x26);
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t data = lwu(bounds + 0x20 * i + 0x10);
            TieSpot t;
            t.o_class = ci->second->o_class;
            float pos[4];
            for (uint32_t l = 0; l < 4; ++l) pos[l] = lwc1(data + 0x30 + 4 * l);
            for (uint32_t l = 0; l < 3; ++l) t.pos[l] = pos[l];
            for (uint32_t r = 0; r < 3; ++r)  // (the rows scaled as the emitters' model / 1024: a world unit at scale 1)
                for (uint32_t l = 0; l < 3; ++l) t.rows[r][l] = lwc1(data + 0x10 * r + 4 * l) * pos[3];
            for (int l = 0; l < 3; ++l) {
                t.lo[l] = lo[l] <= hi[l] ? lo[l] : 0.0f;
                t.hi[l] = lo[l] <= hi[l] ? hi[l] : 0.0f;
            }
            const uint32_t b = bounds + 0x20 * i;
            for (uint32_t l = 0; l < 4; ++l) t.sphere[l] = lwc1(b + 4 * l);
            list.push_back(t);
        }
    }
    const uint32_t shrub_data = lwu(OV(0x0016049cu));
    for (size_t c = 0; c < g_slevel.shrubs.size() && c < L.world->shrubs.size(); ++c) {  // the shrubs: class c is the asset's class c
        const StaticShrubClass& sc = g_slevel.shrubs[c];
        if (sc.packets.empty()) continue;
        const uint32_t bounds = lwu(sc.rec + 0x18), n = lhu(sc.rec + 0x16);
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t b = bounds + 0x20 * i;
            const uint32_t data = shrub_data + 0x40 * lhu(b + 0x18);
            TieSpot t;
            t.shrub = true;
            t.o_class = L.world->shrubs[c].o_class;
            float pos[4];
            for (uint32_t l = 0; l < 4; ++l) pos[l] = lwc1(data + 0x30 + 4 * l);
            for (uint32_t l = 0; l < 3; ++l) t.pos[l] = pos[l];
            for (uint32_t r = 0; r < 3; ++r)
                for (uint32_t l = 0; l < 3; ++l) t.rows[r][l] = lwc1(data + 0x10 * r + 4 * l) * pos[3];
            for (uint32_t l = 0; l < 4; ++l) t.sphere[l] = lwc1(b + 4 * l);
            list.push_back(t);
        }
    }
    return list;
}

// orig: (host) see mrp_lighting.h
bool mrp_world_casters_built() { return g_cast_cascades; }

// orig: (host) see mrp_lighting.h
int mrp_world_emitters_near(const float cam[3], float reach, int max, float* pos_r, float* col) {
    if (!g_level.loaded) return 0;
    static std::mutex build_m;  // (the scene's worker - the casters' slots - and the shadow pass both ask)
    std::lock_guard<std::mutex> lg(build_m);
    if (g_emitters_level != g_level.id) build_emitters();
    static std::vector<std::pair<float, size_t>> near;
    near.clear();
    for (size_t i = 0; i < g_emitters.size(); ++i) {
        const Emitter& e = g_emitters[i];
        const float dx = e.pos[0] - cam[0], dy = e.pos[1] - cam[1], dz = e.pos[2] - cam[2];
        const float d2 = dx * dx + dy * dy + dz * dz;
        const float lim = reach + e.r;
        if (d2 >= lim * lim) continue;
        const float d = std::sqrt(d2) - e.r;
        near.push_back({d, i});
    }
    std::sort(near.begin(), near.end());
    const int n = std::min(max, static_cast<int>(near.size()));
    // (faded in and out, never switched: by the distance to the reach, and to the first light left out when there are more than `max`)
    if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr) {  // (testing: how many are near, how many the frame takes)
        static uint64_t calls = 0;
        if (calls++ % 60 == 0) std::printf("[lights] near the camera %zu, taken %d (max %d)\n", near.size(), n, max);
    }
    constexpr float kFade = 6.0f;
    const float cut = static_cast<int>(near.size()) > max ? near[static_cast<size_t>(max)].first : reach;
    for (int k = 0; k < n; ++k) {
        const float d = near[static_cast<size_t>(k)].first;
        const float f = std::clamp((reach - d) / kFade, 0.0f, 1.0f) * std::clamp((cut - d) / kFade, 0.0f, 1.0f);
        const Emitter& e = g_emitters[near[static_cast<size_t>(k)].second];
        for (int l = 0; l < 3; ++l) {
            pos_r[4 * k + l] = e.pos[l];
            col[4 * k + l] = e.col[l] * f;
        }
        pos_r[4 * k + 3] = e.r;
        col[4 * k + 3] = -1.0f;
    }
    if (std::getenv("RAC1_LIGHTS_JUMPS") != nullptr) {  // (testing: a light whose weight jumps between two calls)
        static std::map<size_t, float> prev;
        std::map<size_t, float> now;
        for (int k = 0; k < n; ++k) {
            const float d = near[static_cast<size_t>(k)].first;
            now[near[static_cast<size_t>(k)].second] = std::clamp((reach - d) / kFade, 0.0f, 1.0f) * std::clamp((cut - d) / kFade, 0.0f, 1.0f);
        }
        int jumps = 0;
        for (const auto& [i, f] : now) {
            const auto it = prev.find(i);
            if (std::fabs(f - (it == prev.end() ? 0.0f : it->second)) > 0.2f) ++jumps;
        }
        for (const auto& [i, f] : prev)
            if (now.count(i) == 0 && f > 0.2f) ++jumps;
        static uint64_t calls = 0;
        if (jumps != 0 && calls > 2) std::printf("[lights] call %llu: %d jumps (%zu near, %d taken, max %d)\n", static_cast<unsigned long long>(calls), jumps, near.size(), n, max);
        ++calls;
        prev = now;
    }
    return n;
}

// orig: (host) see mrp_world_renderer.h
std::vector<std::pair<uint32_t, uint32_t>> mrp_world_skip_ranges() {
    if (!g_frame.skips_ready || g_env_check || g_env_diff) return {};
    return g_frame.skips;
}

// orig: (host) see mrp_world_renderer.h
bool mrp_world_program_start(uint64_t hash, uint32_t start) {
    const int p = program_index(hash);
    if (p >= 0 && g_env_check) scene_check_start(static_cast<Program>(p), start);
    return p >= 0;
}

// orig: (host) see mrp_world_renderer.h
void mrp_world_program_kick(const uint8_t* data, uint32_t qwc) {
    if (g_env_check) scene_check_kick(data, qwc);
}

// orig: (host) see mrp_world_renderer.h
void mrp_world_after_chain(int win_w, int win_h, const gs::Viewport* vp, const gs::Gs& gs, float fb_w, float fb_h, uint64_t frame, const gs::DepthMap* depth) {
    if (!g_frame.active) return;
    if (g_env_check) scene_check_frame(g_frame.scene, g_check);
    if (!g_env_diff || vk::is_active()) return;
    const int vw = vp != nullptr ? vp->w : win_w, vh = vp != nullptr ? vp->h : win_h;
    if (vw <= 0 || vh <= 0) return;
    static GLuint fbo[2] = {}, tex[2] = {}, rbo[2] = {};
    static int fw = 0, fh = 0;
    if (fbo[0] == 0 || fw != vw || fh != vh) {
        for (int i = 0; i < 2; ++i) {
            if (fbo[i] == 0) {
                glGenFramebuffers(1, &fbo[i]);
                glGenTextures(1, &tex[i]);
                rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex[i], "MRP world: check targets (testing)");
                glGenRenderbuffers(1, &rbo[i]);
                rac1::prof::gl_track_n(rac1::prof::kGlRenderbuffer, 1, &rbo[i], "MRP world: check targets (testing)");
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
        if (i == 0) gs::draw(gs, vw, vh, fb_w, fb_h, depth, &lv, 1);  // the faithful world's primitives (marked: mrp_world_testing)
        else draw_frame(&lv, false);  // (the world alone: the faithful side's marked primitives are the world programs')
    }
    std::vector<uint8_t> a(static_cast<size_t>(vw) * vh * 4), b(a.size());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
    glReadPixels(0, 0, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, a.data());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
    glReadPixels(0, 0, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, b.data());
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev_fbo));
    glViewport(vp != nullptr ? vp->x : 0, vp != nullptr ? vp->y : 0, vw, vh);
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
    std::printf("[mrpw-diff] frame %llu (%s projection): faithful %llu px, mrp %llu px, both %llu (colour diff > 8: %llu, mean abs %.2f), only faithful %llu, only mrp %llu\n",
                static_cast<unsigned long long>(frame), g_mrp_world_projection != 0 ? "VU" : "world", static_cast<unsigned long long>(na), static_cast<unsigned long long>(nb),
                static_cast<unsigned long long>(both), static_cast<unsigned long long>(d8), both > 0 ? static_cast<double>(sum) / (3.0 * static_cast<double>(both)) : 0.0,
                static_cast<unsigned long long>(oa), static_cast<unsigned long long>(ob));
    if (const char* shots = std::getenv("RAC1_WORLD_MRP_DIFF_SHOTS")) {  // the two images and their difference at the frames listed
        for (const char* q = shots; *q != 0;) {
            char* e;
            const unsigned long fr = std::strtoul(q, &e, 10);
            if (e == q) break;
            if (fr == frame) {
                for (int i = 0; i < 3; ++i) {
                    char fn[64];
                    std::snprintf(fn, sizeof fn, "mrpw_diff_%lu_%s.ppm", fr, i == 0 ? "faithful" : i == 1 ? "mrp" : "delta");
                    if (FILE* fp = std::fopen(fn, "wb")) {
                        std::fprintf(fp, "P6\n%d %d\n255\n", vw, vh);
                        for (int y = vh - 1; y >= 0; --y)
                            for (int x = 0; x < vw; ++x) {
                                const size_t o = (static_cast<size_t>(y) * vw + x) * 4;
                                uint8_t px[3];
                                for (int c = 0; c < 3; ++c)
                                    px[c] = i == 0 ? a[o + c] : i == 1 ? b[o + c] : static_cast<uint8_t>(std::min(255, 4 * std::abs(static_cast<int>(a[o + c]) - static_cast<int>(b[o + c]))));
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

// orig: (host) see mrp_world_renderer.h
void mrp_world_report() {
    if (!g_world_mrp) return;
    if (scene_capture_misses() != 0) std::printf("[mrpw] WARNING: %llu scene reads not captured\n", static_cast<unsigned long long>(scene_capture_misses()));
    static const char* names[6] = {"tfrag A", "tfrag B", "tie A", "tie B", "shrub A", "shrub B"};
    std::printf("[mrpw] %llu frames drawn, %llu scenes: packets %llu, vertices %llu, triangles %llu (ADC %llu, tie B flagged dropped %llu), batches %llu; set-up starts %llu, unknown programs %llu\n",
                static_cast<unsigned long long>(g_st.frames), static_cast<unsigned long long>(g_st.scene_frames), static_cast<unsigned long long>(g_st.packets),
                static_cast<unsigned long long>(g_st.vertices), static_cast<unsigned long long>(g_st.triangles), static_cast<unsigned long long>(g_st.adc),
                static_cast<unsigned long long>(g_st.clip_dropped), static_cast<unsigned long long>(g_st.batches), static_cast<unsigned long long>(g_st.setups),
                static_cast<unsigned long long>(g_st.unknown_program));
    std::printf("[mrpw] static colours: GPU path instances resident %llu, copied %llu; verified %llu, differing %llu; mirror updates %llu, instances changed %llu, words uploaded %llu\n",
                static_cast<unsigned long long>(g_st.col_resident), static_cast<unsigned long long>(g_st.col_copied), static_cast<unsigned long long>(g_st.col_verified),
                static_cast<unsigned long long>(g_st.col_bad), static_cast<unsigned long long>(g_rcols.updates), static_cast<unsigned long long>(g_rcols.instances_changed),
                static_cast<unsigned long long>(g_rcols.words_uploaded));
    std::printf("[mrpw] static ties / shrubs: host draws %llu / %llu, shrub far variants' quads %llu\n", static_cast<unsigned long long>(g_st.stat_draws[0]),
                static_cast<unsigned long long>(g_st.stat_draws[1]), static_cast<unsigned long long>(g_st.stat_variants));
    if (g_static_check) {
        for (int k = 0; k < 2; ++k)
            std::printf("[mrpw] static check %s: walker draws %llu, found %llu, missing %llu, host extra %llu\n", k == 0 ? "ties" : "shrubs", static_cast<unsigned long long>(g_st.chk_walker[k]),
                        static_cast<unsigned long long>(g_st.chk_found[k]), static_cast<unsigned long long>(g_st.chk_missing[k]), static_cast<unsigned long long>(g_st.chk_extra[k]));
        std::printf("[mrpw] static check values: morph %llu, F %llu, alpha %llu differ; rows beyond 1e-4 relative %llu (worst %.3g)\n", static_cast<unsigned long long>(g_st.chk_morph_bad),
                    static_cast<unsigned long long>(g_st.chk_fog_bad), static_cast<unsigned long long>(g_st.chk_alpha_bad), static_cast<unsigned long long>(g_st.chk_rows_bad), g_st.chk_rows_maxrel);
        std::printf("[mrpw] static check far variants: %llu frames, %llu quads, streams differing %llu\n", static_cast<unsigned long long>(g_st.chk_gif_frames),
                    static_cast<unsigned long long>(g_st.chk_gif_quads), static_cast<unsigned long long>(g_st.chk_gif_bad));
        for (const std::string& n : g_st.chk_notes) std::printf("[mrpw] static check: %s\n", n.c_str());
    }
    std::printf("[mrpw] tfrag: GPU path draws %llu (host-made %llu), CPU reference draws %llu; colours: mirror updates %llu, entries changed %llu, words uploaded %llu\n",
                static_cast<unsigned long long>(g_st.tfrag_gpu_draws), static_cast<unsigned long long>(g_st.tfrag_host), static_cast<unsigned long long>(g_st.tfrag_cpu_draws),
                static_cast<unsigned long long>(g_tcols.updates), static_cast<unsigned long long>(g_tcols.entries_changed), static_cast<unsigned long long>(g_tcols.words_uploaded));
    if (g_st.tfrag_checked != 0)
        std::printf("[mrpw] tfrag selfcheck: %llu vertices compared with the reference on VU1 memory, %llu differ\n", static_cast<unsigned long long>(g_st.tfrag_checked),
                    static_cast<unsigned long long>(g_st.tfrag_bad));
    if (g_st.tv_frames != 0)
        std::printf("[mrpw] tfrag verify (GPU against the CPU reference): %llu frames, %llu vertices (%llu differ: world %llu, STQ %llu, Q %llu, RGBA %llu, F %llu), %llu triangles (%llu "
                    "draws' lists differ, %llu closing vertices' state differ)\n",
                    static_cast<unsigned long long>(g_st.tv_frames), static_cast<unsigned long long>(g_st.tv_verts), static_cast<unsigned long long>(g_st.tv_vbad),
                    static_cast<unsigned long long>(g_st.tv_field[0]), static_cast<unsigned long long>(g_st.tv_field[1]), static_cast<unsigned long long>(g_st.tv_field[2]),
                    static_cast<unsigned long long>(g_st.tv_field[3]), static_cast<unsigned long long>(g_st.tv_field[4]), static_cast<unsigned long long>(g_st.tv_tris),
                    static_cast<unsigned long long>(g_st.tv_tbad), static_cast<unsigned long long>(g_st.tv_field[5]));
    if (g_st.tchk_walker + g_st.tchk_extra != 0)
        std::printf("[mrpw] tfrag check (host decisions against the walker's): walker draws %llu, found %llu (code differing %llu), missing %llu, host extra %llu\n",
                    static_cast<unsigned long long>(g_st.tchk_walker), static_cast<unsigned long long>(g_st.tchk_found), static_cast<unsigned long long>(g_st.tchk_code),
                    static_cast<unsigned long long>(g_st.tchk_missing), static_cast<unsigned long long>(g_st.tchk_extra));
    std::printf("[mrpw] draws:");
    for (int i = 0; i < 6; ++i) std::printf(" %s %llu (%llu not in the assets);", names[i], static_cast<unsigned long long>(g_st.draws[i]), static_cast<unsigned long long>(g_st.unknown[i]));
    std::printf(" textures: %llu found, %llu missing\n", static_cast<unsigned long long>(g_st.tex_ok), static_cast<unsigned long long>(g_st.tex_missing));
    std::printf("[mrpw] sky: %llu frames, %llu packets (%llu VU1 kicks), %llu vertices, %llu triangles, %llu sprites, %llu points / lines not drawn; %llu textures decoded, %llu "
                "triangles without their texture\n",
                static_cast<unsigned long long>(g_st.sky_frames), static_cast<unsigned long long>(g_st.sky_packets), static_cast<unsigned long long>(g_st.sky_kicks),
                static_cast<unsigned long long>(g_st.sky_vertices), static_cast<unsigned long long>(g_st.sky_triangles), static_cast<unsigned long long>(g_st.sky_sprites),
                static_cast<unsigned long long>(g_st.sky_other), static_cast<unsigned long long>(g_st.sky_textures), static_cast<unsigned long long>(g_st.sky_tex_missing));
    std::printf("[mrpw] world GIF vertices (the shrub far variants): %llu packets, %llu vertices, %llu triangles; %llu textures decoded, %llu triangles without their texture\n",
                static_cast<unsigned long long>(g_st.gif_packets), static_cast<unsigned long long>(g_st.gif_vertices), static_cast<unsigned long long>(g_st.gif_triangles),
                static_cast<unsigned long long>(g_st.gif_textures), static_cast<unsigned long long>(g_st.gif_tex_missing));
    if (g_env_check) {
        std::printf("[mrpw-check] the scene's kicks against the faithful programs':\n%s", g_check.summary().c_str());
        for (const std::string& n : g_check.notes) std::printf("[mrpw-check]   %s\n", n.c_str());
    }
    if (g_st.diff_frames != 0) {
        const double n = static_cast<double>(g_st.diff_frames);
        std::printf("[mrpw-diff] %llu frames, per frame: faithful %.0f px, mrp %.0f px, both %.0f (colour diff > 8: %.0f, mean abs %.2f), only faithful %.0f, only mrp %.0f\n",
                    static_cast<unsigned long long>(g_st.diff_frames), static_cast<double>(g_st.px_a) / n, static_cast<double>(g_st.px_b) / n, static_cast<double>(g_st.px_both) / n,
                    static_cast<double>(g_st.px_diff8) / n, g_st.px_both > 0 ? static_cast<double>(g_st.px_sum_abs) / (3.0 * static_cast<double>(g_st.px_both)) : 0.0,
                    static_cast<double>(g_st.px_only_a) / n, static_cast<double>(g_st.px_only_b) / n);
    }
}

}  // namespace rac1
