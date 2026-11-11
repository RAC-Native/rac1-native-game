// The MRP world's scene: see mrp_world_scene.h. Host code.
#include <game/render/mrp_world_scene.h>

#include <formats/host_meshes.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/world_vu1.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/vu/vu1.h>
#include <runtime/vu/vu_interpreter.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

namespace rac1::mrpw {

namespace {

std::vector<PassRange> g_passes;
ChainedStats g_cstats;  // (the chained world's account, RAC1_PERF_WORLD_CHAINED)
// The shadow chains of the frame's passes (mrp_lod.h), by the real range they replace
using Shadow = SceneShadow;
std::vector<Shadow> g_shadows;
uint64_t g_capture_misses = 0;

uint32_t w32(const uint8_t* mem, uint32_t qw, int lane) {
    uint32_t v;
    std::memcpy(&v, mem + (((qw & 0x3ff) << 4) + 4 * static_cast<uint32_t>(lane)), 4);
    return v;
}
void put32(uint8_t* mem, uint32_t qw, int lane, uint32_t v) { std::memcpy(mem + (((qw & 0x3ff) << 4) + 4 * static_cast<uint32_t>(lane)), &v, 4); }
float f32(const uint8_t* mem, uint32_t qw, int lane) {
    const uint32_t u = w32(mem, qw, lane);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
std::array<uint32_t, 4> q128(const uint8_t* mem, uint32_t qw) {
    std::array<uint32_t, 4> r;
    std::memcpy(r.data(), mem + ((qw & 0x3ff) << 4), 16);
    return r;
}
uint64_t r64(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}
// FTOI4 as the VU does (saturated)
int32_t ftoi4(float f) {
    const float v = f * 16.0f;
    if (std::isnan(v)) return 0x7fffffff;
    if (!(v < 2147483648.0f)) return 0x7fffffff;
    if (!(v > -2147483648.0f)) return static_cast<int32_t>(0x80000000u);
    return static_cast<int32_t>(v);
}
uint32_t r32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// the walker's private VU1 (persistent: the chain's VIF state and the programs' constants carry over between frames, as on
// the real VU1)
struct Walker {
    vu1::Vu1 vu;
    bool micro_dirty = true;
    Program prog = Program::None;
    // the EE address each VU1 qword's last UNPACK took its data from, and the packets found by the address of their data (the
    // level's class data does not move: a packet's content found once; checked against the memory's header at each use)
    std::array<uint32_t, 0x400> unpack_src{};
    std::unordered_map<uint64_t, WorldIndex::Hit> memo;
    // (the chained world) the level's tie / shrub classes by their record's address (StaticLevel::gen's), and whether one of its
    // shrub packets has colours that read the output buffers (kTwiceBase)
    uint32_t cls_gen = 0;
    std::unordered_map<uint32_t, uint32_t> tie_cls, shrub_cls;
    bool twice = false;
};
Walker* g_walkers[2] = {};  // (1: the =2 comparison's direct build, RAC1_PERF_DIRECT_WORLD)

// orig: (host) a set-up / constants start run on the ported program (its kicks, none expected, dropped)
void run_program(VUState& st, Program prog, uint32_t start) {
    int n = 0;
    const WorldVu1Program* p = world_vu1_programs(n);
    st.xgkick_hook = [](uint32_t) {};
    VuMicroRun r;
    r.start(st, nullptr, 2048, start, p[static_cast<int>(prog)].fast);
    r.limit_issue(2000000);
    while (r.pipe().issued < 2000000 && r.step()) {
    }
    r.finish();
    st.xgkick_hook = nullptr;
}

// orig: (host) the matrix from a world program's positions before the projection to the world (homogeneous, column-major):
// the instance rows are A M (A the pass's projection, the EE's 0x166d00 for the A programs, 0x166d40 for the clipping ones,
// its qwords A's columns; M the instance's model -> (world - camera) x 1024): world = camera + (A^-1 p).xyz / 1024
// (A's 16 floats and the camera as SceneInput captured them)
std::array<double, 16> view_of_ee(const float a[16], const float cam_in[3]) {
    glm::dmat4 A;
    for (int c = 0; c < 4; ++c) A[c] = glm::dvec4(a[4 * c], a[4 * c + 1], a[4 * c + 2], a[4 * c + 3]);
    const glm::dvec3 cam(cam_in[0], cam_in[1], cam_in[2]);
    glm::dmat4 C(1.0 / 1024.0);
    C[3] = glm::dvec4(cam, 1.0);
    const glm::dmat4 M = std::fabs(glm::determinant(A)) > 1e-30 ? C * glm::inverse(A) : glm::dmat4(0.0);
    std::array<double, 16> r;
    std::memcpy(r.data(), &M[0][0], sizeof r);
    return r;
}
// the tfrag view (VU1 base + 5..8: R0, R1, R2, T on the records' world x 1024 integer positions)
std::array<double, 16> view_of_tfrag(const uint8_t* mem, uint32_t base) {
    glm::dmat4 V;
    for (int c = 0; c < 4; ++c) V[c] = glm::dvec4(f32(mem, base + 5 + static_cast<uint32_t>(c), 0), f32(mem, base + 5 + static_cast<uint32_t>(c), 1), f32(mem, base + 5 + static_cast<uint32_t>(c), 2),
                                                  f32(mem, base + 5 + static_cast<uint32_t>(c), 3));
    glm::dmat4 S(1.0 / 1024.0);
    S[3][3] = 1.0;
    const glm::dmat4 M = std::fabs(glm::determinant(V)) > 1e-30 ? S * glm::inverse(V) : glm::dmat4(0.0);
    std::array<double, 16> r;
    std::memcpy(r.data(), &M[0][0], sizeof r);
    return r;
}

int tfrag_lod(Program prog, uint32_t start) {
    if (prog == Program::TfragB) return start == 0x002 ? 2 : -1;
    switch (start) {
    case 0x006: case 0x00c: return 0;
    case 0x008: case 0x00a: return 1;
    case 0x004: case 0x00e: case 0x010: case 0x012: case 0x014: return 2;
    default: return -1;
    }
}

// (RAC1_MRPW_STATIC_VERIFY=1: the resident colours compared with VU1 memory's at every GPU path instance)
const bool g_static_verify = std::getenv("RAC1_MRPW_STATIC_VERIFY") != nullptr;

struct Build {
    const WorldIndex& idx;
    SceneFrame& f;
    const SceneInput& in;  // (the frame's EE inputs, captured)
    int view_a = -1, view_b = -1;
    std::array<double, 16> last_tfrag{};
    // the packet the last set-up start left in VU1 memory (found once: the instances that follow draw the same packet; any
    // set-up / constants start forgets it)
    WorldIndex::Hit hit;
    bool hit_ok = false;
    int last_tfrag_view = -1;
    int view(bool clip) {
        int& v = clip ? view_b : view_a;
        if (v < 0) {
            v = static_cast<int>(f.views.size());
            f.views.push_back(view_of_ee(clip ? in.a_clip.data() : in.a.data(), in.camera.data()));
        }
        return v;
    }
    const float* scr_scale = nullptr;
    const float* scr_off = nullptr;
    bool kicks = false;  // the GPU path's packets get their kicks too (the check)
    Walker* walker = nullptr;
    // (stage 2) the chain's tie / shrub draws decoded but not drawn (their pass made by the host); the host's emitted;
    // the shrub A fog constants the last shrub draw found (vf30 z / w, vf29 x / y); the check's records of the walker's draws
    bool discard[2] = {}, emitted[2] = {};
    bool shrub_fog_ok = false;
    float shrub_fog[4] = {};
    struct ChkRec {
        uint32_t slot = 0;
        const void* pk = nullptr;
        bool gpu = false;
        float rows[4][4] = {};
        float morph[4] = {};
        uint32_t fog = 0, alpha = 0;
    };
    std::vector<ChkRec> chk[2];
    std::vector<std::array<uint32_t, 4>> chk_gif;  // (the check: the shrub range's DIRECT data, against the host's far variants)
    // the chain's tfrag draws not drawn (the host made the frame's tfrag decisions); emitted once; the walker's (entry, code)
    // for the check (RAC1_MRPW_TFRAG_CHECK)
    bool tfrag_discard = false, tfrag_emitted = false;
    bool tfrag_shadow_done = false;
    std::vector<std::pair<uint32_t, uint8_t>> tchk;
    // (the chained world, RAC1_PERF_WORLD_CHAINED) asked for this build; the range the walk is in has its tie A / shrub A draws
    // from the pass's notes (`chained`), the next of them and their end; each note's packet; the level's texture table in the
    // frame's; a draw start the notes did not have, or notes left over (`ch_bad`: the scene is made again without them)
    bool ch_on = false, chained[2] = {}, ch_bad = false, ch_used = false;
    uint32_t ch_at[2] = {}, ch_end[2] = {};
    // (a level whose shrubs read the tie pass's colour tables: the tie run and the last draw that used each table, 0x346 / 0x386)
    bool ch_tables = false;
    int64_t ch_run = -1, ch_table[2] = {-1, -1};
    std::vector<const StaticPacket*> ch_tie, ch_shrub;
    int64_t ch_tex = -1;
    // (the tfrag range: its draws' entries from the pass's notes - the next, their end, each one's entry of the level's
    // (-1: none) -; `ch_tf_keep`: a level whose shrubs read VU1 memory the tfrags' unpacks leave, which are then walked; the
    // view last made from VU1 memory, and its 16 floats)
    bool ch_tf = false, ch_tf_keep = false;
    uint32_t ch_tf_at = 0, ch_tf_end = 0;
    std::vector<int32_t> ch_tf_cls;
    bool ch_tf_view_ok = false;
    std::array<float, 16> ch_tf_floats{};
    std::array<double, 16> ch_tf_view{};
    void push(Kicked&& k, Program prog, uint32_t start, int view, int table, bool state = false) {
        ScenePacket p;
        if (scr_scale != nullptr) {
            for (int l = 0; l < 3; ++l) {
                p.scr_scale[l] = scr_scale[l];
                p.scr_off[l] = scr_off[l];
            }
            p.scr_k = 1;
        }
        p.k = std::move(k);
        p.prog = prog;
        p.start = static_cast<uint16_t>(start);
        p.seq = f.draw_starts;
        p.view = view;
        p.table = table;
        p.state = state;
        f.packets.push_back(std::move(p));
    }
};

// orig: (host) the check's record of a clipping program's instance draw (its instance by its colours' UNPACK source)
void chk_clip(Build& b, int kind, uint32_t src, const void* pk, const float* morph, uint32_t fog, uint32_t alpha) {
    if (!b.in.stat_check || b.in.colours == nullptr) return;
    const int64_t slot = kind == 0 ? b.in.colours->tie_slot(src) : b.in.colours->shrub_slot(src);
    Build::ChkRec r;
    r.slot = slot < 0 ? 0xffffffffu : static_cast<uint32_t>(slot);
    r.pk = pk;
    if (morph != nullptr) std::memcpy(r.morph, morph, sizeof r.morph);
    r.fog = fog;
    r.alpha = alpha;
    b.chk[kind].push_back(r);
}

// orig: (host) a tie A instance's 64 colours (VU1 `table`, STMOD 1 with the row 0x4b000000: 2^23 + byte) are the resident
// table's when their UNPACK's data was the instance data's +0x40 (the walker notes each UNPACK's EE source): its slot named
// instead of a copy (RAC1_MRPW_STATIC_VERIFY=1: compared word by word with the mirror)
bool tie_resident(Build& b, const uint8_t* mem, uint32_t table, SceneInst& si) {
    const ResidentColours* rc = b.in.colours;
    const int64_t slot = rc != nullptr ? rc->tie_slot(b.walker->unpack_src[table & 0x3ff]) : -1;
    if (slot < 0) {
        ++b.f.copied;
        return false;
    }
    if (g_static_verify) {
        ++b.f.verified;
        bool ok = true;
        for (uint32_t c = 0; c < kTieColours && ok; ++c) {
            const uint32_t m = rc->mirror[static_cast<size_t>(slot) + c];
            for (int l = 0; l < 4; ++l) ok = ok && w32(mem, table + c, l) == (0x4b000000u | ((m >> (8 * l)) & 0xff));
        }
        if (!ok) {
            ++b.f.verify_bad;
            ++b.f.copied;
            return false;
        }
    }
    si.colour_first = static_cast<uint32_t>(slot);
    si.resident = true;
    ++b.f.resident;
    return true;
}
// orig: (host) a shrub A instance's 24 colours (its block + 5 .., the drawn lanes x, y, z & 0xff; + 5 w its alpha) are
// the resident table's when the UNPACK that wrote them came from *(0x1604a0) + 0x60 x index (the qword +5 or +6: the first colour
// may come with the block)
bool shrub_resident(Build& b, const uint8_t* mem, uint32_t block, SceneInst& si) {
    const ResidentColours* rc = b.in.colours;
    int64_t slot = -1;
    if (rc != nullptr) {
        slot = rc->shrub_slot(b.walker->unpack_src[(block + 5) & 0x3ff]);
        if (slot < 0) {
            const uint32_t a6 = b.walker->unpack_src[(block + 6) & 0x3ff];
            if (a6 >= 4) slot = rc->shrub_slot(a6 - 4);
        }
    }
    if (slot < 0) {
        ++b.f.copied;
        return false;
    }
    if (g_static_verify) {
        ++b.f.verified;
        bool ok = true;
        for (uint32_t c = 0; c < kShrubColours && ok; ++c) {
            const uint32_t m = rc->mirror[static_cast<size_t>(slot) + c];
            for (int l = 0; l < 3; ++l) ok = ok && (w32(mem, block + 5 + c, l) & 0xff) == ((m >> (8 * l)) & 0xff);
        }
        if (!ok) {
            ++b.f.verify_bad;
            ++b.f.copied;
            return false;
        }
    }
    si.colour_first = static_cast<uint32_t>(slot);
    si.resident = true;
    ++b.f.resident;
    return true;
}

// orig: (host) shrub A / B's draw (micro 0x063, 1.1): the instances of the packet set up last
void draw_shrub_impl(Build& b, VUState& st, Program prog) {
    uint8_t* mem = st.mem.data();
    const uint32_t base = static_cast<uint32_t>(st.vi[11]) & 0x3ff;
    // the instance block: the program's qword 1 y toggled (0x269 - it), stored back (0x063..0x067)
    const uint32_t block = (0x269u - (w32(mem, 1, 1) & 0xffff)) & 0xffff;
    put32(mem, 1, 1, block);
    if (!b.hit_ok) {  // (by the address of the header's data, else by content)
        const uint64_t key = static_cast<uint64_t>(b.walker->unpack_src[base]) << 2 | 1;
        const auto m = b.walker->memo.find(key);
        if (m != b.walker->memo.end()) {
            const ShrubPacket& q = m->second.world->shrubs[static_cast<size_t>(m->second.cls)].packets[static_cast<size_t>(m->second.packet)];
            bool same = true;
            for (int l = 0; l < 4; ++l) same = same && static_cast<int32_t>(w32(mem, base, l)) == q.header[l];
            if (same) {
                b.hit = m->second;
                b.hit_ok = true;
            }
        }
        if (!b.hit_ok) {
            b.hit_ok = b.idx.find_shrub(mem, base, b.hit);
            if (b.hit_ok && (key >> 2) != 0) b.walker->memo[key] = b.hit;
        }
    }
    if (!b.hit_ok) {
        ++b.f.unknown[static_cast<int>(prog)];
        return;
    }
    const WorldIndex::Hit& hit = b.hit;
    const ShrubPacket& p = hit.world->shrubs[static_cast<size_t>(hit.cls)].packets[static_cast<size_t>(hit.packet)];
    const uint32_t count = w32(mem, block, 0) & 0xffff;
    ShrubConsts c;
    // the constants as the constants start (0x000) loaded them into vf30 / vf29 (shrub B: vf22 / vf23 too)
    c.fog_scale = st.vf[30].f[2];
    c.fog_offset = st.vf[30].f[3];
    c.fog_min = st.vf[29].f[0];
    c.fog_max = st.vf[29].f[1];
    const bool clip = prog == Program::ShrubB;
    if (clip) {
        c.clip = true;
        for (int l = 0; l < 4; ++l) {
            c.scale[l] = st.vf[22].f[l];
            c.offset[l] = st.vf[23].f[l];
        }
    }
    std::vector<std::array<uint32_t, 4>> tex;
    const uint32_t ta = base + 1 + static_cast<uint32_t>(p.header[1]);
    for (uint32_t q = 0; q < 4 * p.textures.size(); ++q) tex.push_back(q128(mem, ta + q));
    const int view = b.view(clip);
    b.scr_scale = clip ? c.scale : nullptr;
    b.scr_off = clip ? c.offset : nullptr;
    // the GPU path (the renderer evaluates the packet's static kick program per instance): not for the packets whose colours
    // read the output buffers (kTwiceBase), nor the clipping program
    bool twice = false;
    for (const ShrubKickVertex& v : p.verts) twice = twice || v.colour >= kTwiceBase;
    const bool gpu = !clip && !twice;
    uint32_t tex_first = 0;
    if (gpu) {
        tex_first = static_cast<uint32_t>(b.f.tex.size());
        b.f.tex.insert(b.f.tex.end(), tex.begin(), tex.end());
    }
    for (uint32_t i = 0; i < count && i < 64; ++i) {
        ShrubInstance in;
        in.block = block + 0x1c * i;
        in.vu_mem = mem;
        for (int r = 0; r < 4; ++r)
            for (int l = 0; l < 4; ++l) in.rows[r][l] = f32(mem, in.block + 1 + static_cast<uint32_t>(r), l);
        for (uint32_t cc = 0; cc < 24; ++cc)
            for (int l = 0; l < 4; ++l) in.colour[cc][l] = static_cast<int32_t>(w32(mem, in.block + 5 + cc, l));
        in.alpha = static_cast<int32_t>(w32(mem, in.block + 5, 3));
        Kicked k;
        if (!gpu || b.kicks) {
            k.want_hp = true;
            shrub_kick(p, in, c, tex, k);
        }
        b.push(std::move(k), prog, 0x063, view, 2);
        if (!gpu) {
            uint32_t src = b.walker->unpack_src[(in.block + 5) & 0x3ff];
            if (b.in.colours != nullptr && b.in.colours->shrub_slot(src) < 0) src = b.walker->unpack_src[(in.block + 6) & 0x3ff] - 4;
            chk_clip(b, 1, src, &p, nullptr, 0, static_cast<uint32_t>(in.alpha) & 0xff);
        }
        if (gpu) {
            ScenePacket& sp = b.f.packets.back();
            sp.shrub = &p;
            sp.tex = tex_first;
            sp.ntex = static_cast<uint32_t>(tex.size());
            sp.inst = static_cast<uint32_t>(b.f.insts.size());
            SceneInst si;
            std::memcpy(si.rows, in.rows, sizeof si.rows);
            if (!shrub_resident(b, mem, in.block, si)) {
                si.colour_first = static_cast<uint32_t>(b.f.colours.size());
                for (uint32_t cc = 0; cc < 24; ++cc)
                    b.f.colours.push_back({static_cast<uint32_t>(in.colour[cc][0]), static_cast<uint32_t>(in.colour[cc][1]), static_cast<uint32_t>(in.colour[cc][2]), 0});
            }
            // (shrub_kick's fog: clamp(T.w x scale + offset, floor, ceiling), FTOI4; the GS's F its bits 4..11)
            float fog = in.rows[3][3] * c.fog_scale;
            fog = fog + c.fog_offset;
            fog = std::max(fog, c.fog_min);
            fog = std::min(fog, c.fog_max);
            si.fog = (static_cast<uint32_t>(ftoi4(fog)) >> 4) & 0xff;
            si.alpha = static_cast<uint32_t>(in.alpha) & 0xff;
            b.f.insts.push_back(si);
        }
    }
    b.scr_scale = b.scr_off = nullptr;
    ++b.f.draws[static_cast<int>(prog)];
}

// orig: (host) what a tie draw leaves after its packet: tie A's alpha-test state (the instance's `state` against the program's at
// 0x3cb y: when they differ the A+D packet at 0x3d0 - the tag 0x3cc, TEST 0x3ce (a state) or 0x3cd (none) - kicked before the
// draw's, `drawn`: the draw pushed a packet), then the block queue vf22 rotated (MR32) and vi3 from it
void tie_draw_end(Build& b, VUState& st, Program prog, uint32_t state, bool drawn) {
    uint8_t* mem = st.mem.data();
    if (prog != Program::TieB) {
        const uint32_t prev = w32(mem, 0x3cb, 1) & 0xffff;
        if (state != prev) {
            Kicked s;
            s.q.push_back(q128(mem, 0x3cc));
            s.q.push_back(q128(mem, state != 0 ? 0x3ce : 0x3cd));
            s.part = {0, 1};
            s.clip = {0, 0};
            s.want_hp = true;
            s.hp.assign(2, std::array<float, 4>{});
            // (kicked first: put before the draw's packet)
            b.push(std::move(s), prog, 0x000, -1, 1, true);
            if (drawn && b.f.packets.size() >= 2) std::rotate(b.f.packets.end() - 2, b.f.packets.end() - 1, b.f.packets.end());
            put32(mem, 0x3cb, 1, state);
        }
    }
    VUVector& v = st.vf[22];
    const VUVector o = v;
    v.u[0] = o.u[1];
    v.u[1] = o.u[2];
    v.u[2] = o.u[3];
    v.u[3] = o.u[0];
    st.vi[3] = static_cast<int16_t>(v.u[0] & 0xffff);
}

// orig: (host) tie A / B's draw (micro 0x000), then what the program leaves for the next start: the block
// queue vf22 rotated (MR32) and vi3 from it (0x3cb / 0x3cf, B 0x648 / 0x64c); tie A's alpha-test state (0x3bc..0x3c8)
void draw_tie_impl(Build& b, VUState& st, Program prog) {
    uint8_t* mem = st.mem.data();
    const uint32_t block = static_cast<uint32_t>(st.vi[3]) & 0x3ff;
    const bool clip = prog == Program::TieB;
    // tie A: the instance's state (+5 x) stored at 0x3cb x (0x098 / 0x09d)
    const uint32_t state = w32(mem, block + 5, 0) & 0xffff;
    if (!clip) put32(mem, 0x3cb, 0, state);
    auto finish = [&](bool drawn) { tie_draw_end(b, st, prog, state, drawn); };
    if (!b.hit_ok) {  // (by the address of the vertices' data, else by content)
        const uint64_t key = static_cast<uint64_t>(b.walker->unpack_src[0x32]) << 2 | 2;
        const auto m = b.walker->memo.find(key);
        if (m != b.walker->memo.end()) {
            const TiePacket& q = m->second.world->ties[static_cast<size_t>(m->second.cls)].packets[static_cast<size_t>(m->second.packet)];
            if (q.image.size() >= 16 && std::memcmp(q.image.data(), mem, 16) == 0) {
                b.hit = m->second;
                b.hit_ok = true;
            }
        }
        if (!b.hit_ok) {
            b.hit_ok = b.idx.find_tie(mem, b.hit);
            if (b.hit_ok && (key >> 2) != 0) b.walker->memo[key] = b.hit;
        }
    }
    if (!b.hit_ok) {
        ++b.f.unknown[static_cast<int>(prog)];
        finish(false);
        return;
    }
    const WorldIndex::Hit& hit = b.hit;
    const TiePacket& p = hit.world->ties[static_cast<size_t>(hit.cls)].packets[static_cast<size_t>(hit.packet)];
    const uint32_t table = block == 0xf2 ? 0x386 : 0x346;
    if (!clip && !b.kicks) {  // the GPU path alone: the instance's data copied as VU1 memory holds it
        b.push(Kicked{}, prog, 0x000, b.view(false), 1);
        ScenePacket& sp = b.f.packets.back();
        sp.tie = &p;
        sp.tex = static_cast<uint32_t>(b.f.tex.size());
        sp.ntex = static_cast<uint32_t>(5 * p.prims.size());
        b.f.tex.resize(b.f.tex.size() + sp.ntex);
        std::memcpy(b.f.tex[sp.tex].data(), mem + 16, 16u * sp.ntex);
        sp.inst = static_cast<uint32_t>(b.f.insts.size());
        SceneInst& si = b.f.insts.emplace_back();
        std::memcpy(si.rows, mem + 16 * block, sizeof si.rows);
        std::memcpy(si.morph, mem + 16 * (block + 4), sizeof si.morph);
        if (!tie_resident(b, mem, table, si)) {
            si.colour_first = static_cast<uint32_t>(b.f.colours.size());
            b.f.colours.resize(b.f.colours.size() + 64);
            std::memcpy(b.f.colours[si.colour_first].data(), mem + 16 * table, 64 * 16);
        }
        si.fog = (static_cast<uint32_t>(ftoi4(f32(mem, block + 5, 3))) >> 4) & 0xff;  // (tie_kick's F: FTOI4 of the block's +5 w)
        ++b.f.draws[static_cast<int>(prog)];
        finish(true);
        return;
    }
    TieInstance in;
    for (int r = 0; r < 4; ++r)
        for (int l = 0; l < 4; ++l) in.rows[r][l] = f32(mem, block + static_cast<uint32_t>(r), l);
    for (int l = 0; l < 4; ++l) {
        in.morph[l] = f32(mem, block + 4, l);
        in.fog[l] = f32(mem, block + 5, l);
    }
    for (uint32_t c = 0; c < 64; ++c)
        for (int l = 0; l < 4; ++l) in.colour[c][l] = w32(mem, table + c, l);
    std::vector<std::array<uint32_t, 4>> tex;
    for (uint32_t q = 0; q < 5 * p.prims.size(); ++q) tex.push_back(q128(mem, 1 + q));
    if (clip) {
        in.clip = true;
        for (int l = 0; l < 4; ++l) {
            in.scale[l] = f32(mem, 0x3ce, l);
            in.offset[l] = f32(mem, 0x3cd, l);
        }
    }
    Kicked k;
    const bool gpu = !clip;  // (the GPU path: tie A; tie B drops the triangles its clip test flags, a decision per triangle)
    if (!gpu || b.kicks) {
        k.want_hp = true;
        tie_kick(p, in, tex, k);
    }
    b.scr_scale = clip ? in.scale : nullptr;
    b.scr_off = clip ? in.offset : nullptr;
    b.push(std::move(k), prog, 0x000, b.view(clip), 1);
    if (clip) chk_clip(b, 0, b.walker->unpack_src[table & 0x3ff], &p, in.morph, (static_cast<uint32_t>(ftoi4(in.fog[3])) >> 4) & 0xff, 0);
    b.scr_scale = b.scr_off = nullptr;
    if (gpu) {
        ScenePacket& sp = b.f.packets.back();
        sp.tie = &p;
        sp.tex = static_cast<uint32_t>(b.f.tex.size());
        sp.ntex = static_cast<uint32_t>(tex.size());
        b.f.tex.insert(b.f.tex.end(), tex.begin(), tex.end());
        sp.inst = static_cast<uint32_t>(b.f.insts.size());
        SceneInst si;
        std::memcpy(si.rows, in.rows, sizeof si.rows);
        std::memcpy(si.morph, in.morph, sizeof si.morph);
        if (!tie_resident(b, mem, table, si)) {
            si.colour_first = static_cast<uint32_t>(b.f.colours.size());
            for (uint32_t c = 0; c < 64; ++c) b.f.colours.push_back({in.colour[c][0], in.colour[c][1], in.colour[c][2], in.colour[c][3]});
        }
        si.fog = (static_cast<uint32_t>(ftoi4(in.fog[3])) >> 4) & 0xff;  // (tie_kick's F: FTOI4 of the block's +5 w)
        b.f.insts.push_back(si);
    }
    ++b.f.draws[static_cast<int>(prog)];
    finish(true);
}

// orig: (host) a tie / shrub draw decoded; when its pass is made by the host (b.discard) what it pushed is dropped (its
// effects on the private VU1 kept); with the check the instances it drew recorded (the GPU path's from their records, the clipping
// programs' by the colours' source)
template <class F>
void draw_wrapped(Build& b, int kind, F&& draw) {
    const size_t np = b.f.packets.size(), ni = b.f.insts.size(), nc = b.f.colours.size(), nt = b.f.tex.size();
    draw();
    if (b.in.stat_check)
        for (size_t i = np; i < b.f.packets.size(); ++i) {
            const ScenePacket& p = b.f.packets[i];
            if (p.state || (p.tie == nullptr && p.shrub == nullptr) || p.inst >= b.f.insts.size()) continue;
            const SceneInst& si = b.f.insts[p.inst];
            Build::ChkRec r;
            r.slot = si.resident ? si.colour_first : 0xffffffffu;
            r.pk = kind == 0 ? static_cast<const void*>(p.tie) : static_cast<const void*>(p.shrub);
            r.gpu = true;
            std::memcpy(r.rows, si.rows, sizeof r.rows);
            std::memcpy(r.morph, si.morph, sizeof r.morph);
            r.fog = si.fog;
            r.alpha = si.alpha;
            b.chk[kind].push_back(r);
        }
    if (b.discard[kind]) {
        b.f.packets.resize(np);
        b.f.insts.resize(ni);
        b.f.colours.resize(nc);
        b.f.tex.resize(nt);
    }
}
void draw_shrub(Build& b, VUState& st, Program prog) {
    if (prog == Program::ShrubA) {  // (the fog constants the host's shrubs take)
        b.shrub_fog[0] = st.vf[30].f[2];
        b.shrub_fog[1] = st.vf[30].f[3];
        b.shrub_fog[2] = st.vf[29].f[0];
        b.shrub_fog[3] = st.vf[29].f[1];
        b.shrub_fog_ok = true;
    }
    if (b.discard[1]) {  // (the host draws this pass: only what the draw leaves for the next starts, the instance block toggle 0x063..0x067)
        uint8_t* mem = st.mem.data();
        put32(mem, 1, 1, (0x269u - (w32(mem, 1, 1) & 0xffff)) & 0xffff);
        return;
    }
    draw_wrapped(b, 1, [&] { draw_shrub_impl(b, st, prog); });
}
void draw_tie(Build& b, VUState& st, Program prog) {
    if (b.discard[0]) {  // (the host draws this pass: only what the draw leaves, the alpha-test state 0x3cb and the block queue vf22 / vi3)
        uint8_t* mem = st.mem.data();
        const uint32_t block = static_cast<uint32_t>(st.vi[3]) & 0x3ff;
        if (prog != Program::TieB) {
            const uint32_t state = w32(mem, block + 5, 0) & 0xffff;
            put32(mem, 0x3cb, 0, state);
            if (state != (w32(mem, 0x3cb, 1) & 0xffff)) put32(mem, 0x3cb, 1, state);
        }
        VUVector& v = st.vf[22];
        const VUVector o = v;
        v.u[0] = o.u[1];
        v.u[1] = o.u[2];
        v.u[2] = o.u[3];
        v.u[3] = o.u[0];
        st.vi[3] = static_cast<int16_t>(v.u[0] & 0xffff);
        return;
    }
    draw_wrapped(b, 0, [&] { draw_tie_impl(b, st, prog); });
}

// ---- the chained world (mrp_world_scene.h, RAC1_PERF_WORLD_CHAINED) ------------------------------------------------------------

// orig: (host) the walk enters the tfrag pass's range that starts at `first`: its draws' entries come from the pass's notes when
// that run's stand, the GPU path draws the tfrags and every note names an entry of the level's; else the range is walked as
// before. In a level whose shrubs read VU1 memory the tfrags' unpacks leave (chained_enter) the unpacks are walked all the same
void chained_level(Build& b);
void chained_tfrag_enter(Build& b, uint32_t first) {
    b.ch_tf = false;
    static const bool selfcheck = std::getenv("RAC1_MRPW_TFRAG_SELFCHECK") != nullptr;
    const ChainedWorld& cw = b.in.chained;
    const TfragLevel* L = b.in.tfrag_level;
    const StaticLevel* sl = b.in.stat_level;
    if (!b.ch_on || L == nullptr || sl == nullptr || !sl->ok || b.tfrag_discard || b.in.tfrag_kicks || selfcheck) return;
    const WorldChained::Run* run = nullptr;
    for (const WorldChained::Run& r : cw.notes.runs)
        if (r.kind == 0 && r.first == first) run = &r;
    bool ok = run != nullptr && run->ok;
    if (ok) {
        if (b.ch_tf_cls.size() < cw.notes.tfrag.size()) b.ch_tf_cls.resize(cw.notes.tfrag.size(), -1);
        for (uint32_t i = run->at; ok && i < run->at + run->n; ++i) {
            const uint32_t k = (cw.notes.tfrag[i].entry - L->table) / 0x40;
            ok = cw.notes.tfrag[i].entry >= L->table && k < L->by_ee.size() && L->by_ee[k] >= 0;
            if (ok) b.ch_tf_cls[i] = L->by_ee[k];
        }
    }
    if (!ok) {
        ++g_cstats.walked[0];
        return;
    }
    chained_level(b);
    b.ch_tf = true;
    b.ch_tf_keep = b.walker->twice;
    b.ch_used = true;
    b.ch_tf_at = run->at;
    b.ch_tf_end = run->at + run->n;
    b.ch_tf_view_ok = false;
}

// orig: (host) the walk leaves that range: every note was a draw start of the chain's
void chained_tfrag_leave(Build& b) {
    if (b.ch_tf && b.ch_tf_at != b.ch_tf_end) b.ch_bad = true;
    b.ch_tf = false;
}

bool tfrag_gpu_draw(Build& b, const uint8_t* mem, uint32_t base, Program prog, uint32_t start, int cls, int lod);
// orig: (host) a tfrag draw start of the chain (tfrag A's LOD starts, tfrag B's 0x002) made from the pass's note of it: the
// entry is the note's - draw_tfrag finds it by a hash of the image its unpacks left -, the LOD and the program the chain's own
// start; the view from VU1 memory at the double buffer's base (made again only when its floats changed), the GPU path's record
// as draw_tfrag makes it. A note that is not this start's kind (a clipped entry for tfrag B), or an entry the GPU path has no
// plan for (the walk draws it with the CPU reference, from VU1 memory): the scene is made again by the walk
void draw_tfrag_chained(Build& b, VUState& st, Program prog, uint32_t start) {
    if (b.ch_tf_at >= b.ch_tf_end) {
        b.ch_bad = true;
        return;
    }
    const uint32_t i = b.ch_tf_at++;
    const bool isb = prog == Program::TfragB;
    if (isb != (b.in.chained.notes.tfrag[i].lod == 2)) {
        b.ch_bad = true;
        return;
    }
    const uint8_t* mem = st.mem.data();
    const int lod = tfrag_lod(prog, start);
    const uint32_t base = st.xtop & 0x3ff;
    std::array<float, 16> fl;
    std::memcpy(fl.data(), mem + (((base + 5) & 0x3ff) << 4), sizeof fl);
    if (!b.ch_tf_view_ok || std::memcmp(fl.data(), b.ch_tf_floats.data(), sizeof fl) != 0) {
        b.ch_tf_floats = fl;
        b.ch_tf_view = view_of_tfrag(mem, base);
        b.ch_tf_view_ok = true;
    }
    if (b.last_tfrag_view < 0 || b.ch_tf_view != b.last_tfrag) {
        b.last_tfrag = b.ch_tf_view;
        b.last_tfrag_view = static_cast<int>(b.f.views.size());
        b.f.views.push_back(b.ch_tf_view);
    }
    if (!tfrag_gpu_draw(b, mem, base, prog, start, b.ch_tf_cls[i], lod)) {
        b.ch_bad = true;
        return;
    }
    ++b.f.draws[static_cast<int>(prog)];
    ++g_cstats.tfrags;
}

// orig: (host) the level's texture table's place in the frame's (StaticLevel::tex: kept at the frame's start, or put in now)
uint32_t chained_tex_base(Build& b) {
    if (b.ch_tex < 0) {
        const StaticLevel* sl = b.in.stat_level;
        if (b.f.static_tex_base >= 0 && b.f.static_tex_gen == sl->gen && b.f.static_tex_n == sl->tex.size()) {
            b.ch_tex = b.f.static_tex_base;
        } else {
            b.ch_tex = static_cast<int64_t>(b.f.tex.size());
            b.f.tex.insert(b.f.tex.end(), sl->tex.begin(), sl->tex.end());
        }
    }
    return static_cast<uint32_t>(b.ch_tex);
}

// orig: (host) the walk enters the tie (kind 0) / shrub (1) pass's range that starts at `first`: its draws come from the pass's
// notes when that run's stand and every one names a packet of the level's tables and colours of the resident table; else the
// range is walked as before. In a level with a shrub packet whose colours read VU1 memory past its instance's block (kTwiceBase:
// the CPU path draws it with what the private VU1 holds there - the shrub pass's earlier unpacks, the tie pass's colour tables)
// the shrub pass is walked whole and the tie A part's colour tables are kept (chained_tables)
void chained_level(Build& b) {
    const StaticLevel* sl = b.in.stat_level;
    Walker& W = *b.walker;
    if (W.cls_gen == sl->gen) return;
    W.cls_gen = sl->gen;
    W.tie_cls.clear();
    W.shrub_cls.clear();
    W.twice = false;
    for (size_t c = 0; c < sl->ties.size(); ++c) W.tie_cls.emplace(sl->ties[c].rec, static_cast<uint32_t>(c));
    for (size_t c = 0; c < sl->shrubs.size(); ++c) {
        W.shrub_cls.emplace(sl->shrubs[c].rec, static_cast<uint32_t>(c));
        for (const StaticPacket& pk : sl->shrubs[c].packets)
            for (const ShrubKickVertex& v : pk.shrub->verts) W.twice = W.twice || v.colour >= kTwiceBase;
    }
}
void chained_enter(Build& b, int kind, uint32_t first) {
    b.chained[kind] = false;
    const ChainedWorld& cw = b.in.chained;
    const StaticLevel* sl = b.in.stat_level;
    const ResidentColours* rc = b.in.colours;
    if (!b.ch_on || sl == nullptr || !sl->ok || rc == nullptr || b.discard[kind]) return;
    const WorldChained::Run* run = nullptr;
    for (const WorldChained::Run& r : cw.notes.runs)
        if (r.kind == kind + 1 && r.first == first) run = &r;
    bool ok = run != nullptr && run->ok;
    Walker& W = *b.walker;
    if (ok) chained_level(b);
    ok = ok && !(kind == 1 && W.twice);
    if (ok && kind == 0) {
        if (b.ch_tie.size() < cw.notes.tie.size()) b.ch_tie.resize(cw.notes.tie.size(), nullptr);
        uint32_t cls = 0;
        const StaticTieClass* sc = nullptr;
        for (uint32_t i = run->at; ok && i < run->at + run->n; ++i) {
            const WorldChained::Tie& t = cw.notes.tie[i];
            if (sc == nullptr || t.cls != cls) {
                const auto it = W.tie_cls.find(t.cls);
                sc = it != W.tie_cls.end() ? &sl->ties[it->second] : nullptr;
                cls = t.cls;
            }
            ok = sc != nullptr && t.lod < 3 && t.packet < sc->lod[t.lod].size() && rc->tie_slot(t.colours) >= 0;
            if (ok) b.ch_tie[i] = &sc->lod[t.lod][t.packet];
        }
    }
    if (ok && kind == 1) {
        if (b.ch_shrub.size() < cw.notes.shrub.size()) b.ch_shrub.resize(cw.notes.shrub.size(), nullptr);
        uint32_t cls = 0;
        const StaticShrubClass* sc = nullptr;
        for (uint32_t i = run->at; ok && i < run->at + run->n; ++i) {
            const WorldChained::Shrub& g = cw.notes.shrub[i];
            if (sc == nullptr || g.cls != cls) {
                const auto it = W.shrub_cls.find(g.cls);
                sc = it != W.shrub_cls.end() ? &sl->shrubs[it->second] : nullptr;
                cls = g.cls;
            }
            ok = sc != nullptr && g.packet < sc->packets.size();
            if (!ok) break;
            for (uint32_t k = 0; ok && k < g.n; ++k) ok = rc->shrub_slot(g.colours[k]) >= 0;
            if (ok) b.ch_shrub[i] = &sc->packets[g.packet];
        }
    }
    if (!ok) {
        ++g_cstats.walked[kind + 1];
        return;
    }
    b.chained[kind] = true;
    b.ch_used = true;
    b.ch_at[kind] = run->at;
    b.ch_end[kind] = run->at + run->n;
    if (kind == 0) {
        b.ch_tables = W.twice;
        b.ch_run = run - cw.notes.runs.data();
        b.ch_table[0] = b.ch_table[1] = -1;
    }
}

// orig: (host) the tie A part's colour tables (VU1 0x346 / 0x386: 64 quadwords each, a colour's bytes as words on the row
// 0x4b000000) as its last draws' unpacks left them - the last draw that used each table, from the capture's copy of the run's
// last two draws' colours -, for the shrub packets that read that memory (Build::ch_tables). Before the pass's next part is
// walked, and at the range's end
void chained_tables(Build& b, VUState& st) {
    if (!b.ch_tables || b.ch_run < 0) return;
    const ChainedWorld& cw = b.in.chained;
    const WorldChained::Run& run = cw.notes.runs[static_cast<size_t>(b.ch_run)];
    uint8_t* mem = st.mem.data();
    for (int t = 0; t < 2; ++t) {
        const int64_t i = b.ch_table[t];
        if (i < 0) continue;
        b.ch_table[t] = -1;
        const int64_t back = static_cast<int64_t>(run.at) + run.n - 1 - i;  // (0: the run's last draw, 1: the one before)
        if (back < 0 || back > 1) {
            b.ch_bad = true;
            continue;
        }
        const uint8_t* col = cw.tie_tail[static_cast<size_t>(b.ch_run)].data() + 256 * back;
        const uint32_t table = t == 0 ? 0x346 : 0x386;
        for (uint32_t c = 0; c < kTieColours; ++c)
            for (int l = 0; l < 4; ++l) put32(mem, table + c, l, 0x4b000000u | col[4 * c + static_cast<uint32_t>(l)]);
    }
}

// orig: (host) the walk leaves that range: every note was a draw start of the chain's
void chained_leave(Build& b, VUState& st, int kind) {
    if (b.chained[kind] && b.ch_at[kind] != b.ch_end[kind]) b.ch_bad = true;
    if (b.chained[kind] && kind == 0) chained_tables(b, st);
    b.chained[kind] = false;
}

// orig: (host) a tie A draw start of the chain (micro 0x000) made from the pass's note of it: the GPU path's packet as
// draw_tie_impl makes it from VU1 memory - the class's packet and its texture qwords from the level's tables, the instance's rows,
// morph, state and fog from its 0x60 bytes of the frame packet, its colours' resident slot - and what the draw leaves (tie_draw_end)
void draw_tie_chained(Build& b, VUState& st) {
    if (b.ch_at[0] >= b.ch_end[0]) {
        b.ch_bad = true;
        return;
    }
    const ChainedWorld& cw = b.in.chained;
    const uint32_t i = b.ch_at[0]++;
    const StaticPacket* pk = b.ch_tie[i];
    const uint8_t* d = cw.tie_bytes.data() + 0x60 * static_cast<size_t>(i);
    uint8_t* mem = st.mem.data();
    const uint32_t state = r32(d + 0x50) & 0xffff;
    put32(mem, 0x3cb, 0, state);
    b.ch_table[(static_cast<uint32_t>(st.vi[3]) & 0x3ff) == 0xf2 ? 1 : 0] = i;  // (the block the program's queue gives this draw: its table)
    const uint32_t tex = chained_tex_base(b);
    b.push(Kicked{}, Program::TieA, 0x000, b.view(false), 1);
    ScenePacket& sp = b.f.packets.back();
    sp.tie = pk->tie;
    sp.tex = tex + pk->tex_first;
    sp.ntex = pk->ntex;
    sp.inst = static_cast<uint32_t>(b.f.insts.size());
    SceneInst si;
    std::memcpy(si.rows, d, sizeof si.rows);
    std::memcpy(si.morph, d + 0x40, sizeof si.morph);
    si.colour_first = static_cast<uint32_t>(b.in.colours->tie_slot(cw.notes.tie[i].colours));
    si.resident = true;
    ++b.f.resident;
    float fog;
    std::memcpy(&fog, d + 0x5c, 4);
    si.fog = (static_cast<uint32_t>(ftoi4(fog)) >> 4) & 0xff;  // (tie_kick's F: FTOI4 of the block's +5 w)
    b.f.insts.push_back(si);
    ++b.f.draws[static_cast<int>(Program::TieA)];
    ++g_cstats.ties;
    tie_draw_end(b, st, Program::TieA, state, true);
}

// orig: (host) a shrub A draw start (micro 0x063) made from the pass's note of it: its instances' GPU path packets as
// draw_shrub_impl makes them - the rows from each one's 0x50 bytes of the frame packet, the fog from the constants the pass's
// start left (vf30 / vf29), the alpha as the program reads it - and the instance block's toggle the draw leaves
void draw_shrub_chained(Build& b, VUState& st) {
    uint8_t* mem = st.mem.data();
    b.shrub_fog[0] = st.vf[30].f[2];
    b.shrub_fog[1] = st.vf[30].f[3];
    b.shrub_fog[2] = st.vf[29].f[0];
    b.shrub_fog[3] = st.vf[29].f[1];
    b.shrub_fog_ok = true;
    put32(mem, 1, 1, (0x269u - (w32(mem, 1, 1) & 0xffff)) & 0xffff);
    if (b.ch_at[1] >= b.ch_end[1]) {
        b.ch_bad = true;
        return;
    }
    const ChainedWorld& cw = b.in.chained;
    const uint32_t i = b.ch_at[1]++;
    const WorldChained::Shrub& g = cw.notes.shrub[i];
    const StaticPacket* pk = b.ch_shrub[i];
    const uint32_t tex = chained_tex_base(b);
    const int view = b.view(false);
    for (uint32_t k = 0; k < g.n; ++k) {
        const size_t at = static_cast<size_t>(cw.shrub_first[i]) + k;
        const uint8_t* d = cw.shrub_bytes.data() + 0x50 * at;
        b.push(Kicked{}, Program::ShrubA, 0x063, view, 2);
        ScenePacket& sp = b.f.packets.back();
        sp.shrub = pk->shrub;
        sp.tex = tex + pk->tex_first;
        sp.ntex = pk->ntex;
        sp.inst = static_cast<uint32_t>(b.f.insts.size());
        SceneInst si;
        std::memcpy(si.rows, d, sizeof si.rows);
        si.colour_first = static_cast<uint32_t>(b.in.colours->shrub_slot(g.colours[k]));
        si.resident = true;
        ++b.f.resident;
        // (shrub_kick's fog: clamp(T.w x scale + offset, floor, ceiling), FTOI4; the GS's F its bits 4..11)
        float fog = si.rows[3][3] * b.shrub_fog[0];
        fog = fog + b.shrub_fog[1];
        fog = std::max(fog, b.shrub_fog[2]);
        fog = std::min(fog, b.shrub_fog[3]);
        si.fog = (static_cast<uint32_t>(ftoi4(fog)) >> 4) & 0xff;
        si.alpha = cw.shrub_alpha[at];
        b.f.insts.push_back(si);
        ++g_cstats.shrubs;
    }
    ++b.f.draws[static_cast<int>(Program::ShrubA)];
}

void emit_variants(Build& b);

// orig: (host) the host's ties (kind 0) / shrubs (1) of the frame drawn where the pass's range ends, as the GPU path's
// packets (the instance's rows, morph, F, alpha; its resident colours; the class's texture prims as VU1 memory would hold them);
// a tie A alpha-test state packet first when the program's state (0x3cb y) is not the host instances' 0; after the shrubs their
// far variants' quads (emit_variants)
void emit_static(Build& b, VUState& st, int kind) {
    const StaticFrame* sf = b.in.stat;
    const StaticLevel* sl = b.in.stat_level;
    if (sf == nullptr || sl == nullptr || b.in.stat_check || !sf->on[kind] || b.emitted[kind]) return;
    b.emitted[kind] = true;
    uint8_t* mem = st.mem.data();
    if (kind == 0 && (w32(mem, 0x3cb, 1) & 0xffff) != 0 && !sf->draws[0].empty()) {
        Kicked s;
        s.q.push_back(q128(mem, 0x3cc));
        s.q.push_back(q128(mem, 0x3cd));
        s.part = {0, 1};
        s.clip = {0, 0};
        s.want_hp = true;
        s.hp.assign(2, std::array<float, 4>{});
        b.push(std::move(s), Program::TieA, 0x000, -1, 1, true);
    }
    if (kind == 1 && !b.shrub_fog_ok) {  // (no shrub draw this frame: the registers the pass's constants start left)
        b.shrub_fog[0] = st.vf[30].f[2];
        b.shrub_fog[1] = st.vf[30].f[3];
        b.shrub_fog[2] = st.vf[29].f[0];
        b.shrub_fog[3] = st.vf[29].f[1];
    }
    uint32_t tex_base = static_cast<uint32_t>(b.f.tex.size());
    if (b.f.static_tex_base >= 0 && b.f.static_tex_gen == sl->gen && b.f.static_tex_n == sl->tex.size())
        tex_base = static_cast<uint32_t>(b.f.static_tex_base);  // (the level's table kept in the frame: scene_build)
    else b.f.tex.insert(b.f.tex.end(), sl->tex.begin(), sl->tex.end());
    std::vector<int32_t> map(sf->insts.size(), -1);
    const int view = b.view(false);
    for (const StaticDraw& d : sf->draws[kind]) {
        const StaticInst& si = sf->insts[d.inst];
        if (map[d.inst] < 0) {
            map[d.inst] = static_cast<int32_t>(b.f.insts.size());
            SceneInst x;
            std::memcpy(x.rows, si.rows, sizeof x.rows);
            std::memcpy(x.morph, si.morph, sizeof x.morph);
            x.colour_first = si.slot;
            x.resident = true;
            x.alpha = si.alpha;
            if (kind == 0) {
                x.fog = si.fog;
            } else {  // (shrub_kick's F: clamp(T.w x scale + offset, floor, ceiling), FTOI4)
                float fog = si.rows[3][3] * b.shrub_fog[0];
                fog = fog + b.shrub_fog[1];
                fog = std::max(fog, b.shrub_fog[2]);
                fog = std::min(fog, b.shrub_fog[3]);
                x.fog = (static_cast<uint32_t>(ftoi4(fog)) >> 4) & 0xff;
            }
            b.f.insts.push_back(x);
        }
        b.push(Kicked{}, kind == 0 ? Program::TieA : Program::ShrubA, kind == 0 ? 0x000 : 0x063, view, kind == 0 ? 1 : 2);
        ScenePacket& sp = b.f.packets.back();
        sp.tie = d.pk->tie;
        sp.shrub = d.pk->shrub;
        sp.tex = tex_base + d.pk->tex_first;
        sp.ntex = d.pk->ntex;
        sp.inst = static_cast<uint32_t>(map[d.inst]);
        ++b.f.stat_draws[kind];
    }
    if (kind == 1) emit_variants(b);
}

// orig: (host) a level mod's host meshes (src/formats/host_meshes.h): no EE data stands for them, so the host makes their draws, after the world passes, as the GPU path's tie A packets: per instance near enough and not behind
// the camera (its class's sphere in the view, as the tie cull's view depth), its rows as the tie pass makes them (instance_rows:
// the projection A times the rotation x scale, (position - camera) x 1024), LOD 0 (the near path's morph: the fat vertices'
// base), the tie pass's fog at its depth, its palette (its vertices' colours) then its one colour in the 64 entries; first the tie pass's alpha-test state (TEST_1
// 0x5340b, its constants' qword 0x3cd)
void emit_host_meshes(Build& b) {
    const World* w = host_meshes_bound(&b.idx);
    if (w == nullptr || (w->host_meshes.empty() && b.in.host_dyn.empty())) return;
    float tie_fog[4];
    host_meshes_fog(tie_fog);
    const SceneInput& in = b.in;
    const float* A = in.a.data();
    {
        Kicked s;
        s.q.push_back({0x00008001u, 0x10000000u, 0x0000000eu, 0u});
        s.q.push_back({0x0005340bu, 0u, 0x00000047u, 0u});
        s.part = {0, 1};
        s.clip = {0, 0};
        s.want_hp = true;
        s.hp.assign(2, std::array<float, 4>{});
        b.push(std::move(s), Program::TieA, 0x000, -1, 1, true);
    }
    static const float far_cull = [] {
        const char* e = std::getenv("RAC1_HOST_MESH_FAR");
        return e != nullptr ? static_cast<float>(std::atof(e)) : 1000.0f;
    }();  // (0: none culled)
    const int view = b.view(false);
    const size_t n_static = w->host_meshes.size();
    for (size_t ii = 0; ii < n_static + in.host_dyn.size(); ++ii) {
        // (the static ones, then the level's objects': src/formats/host_meshes.h host_meshes_dynamic)
        const HostMeshInstance& ci = ii < n_static ? w->host_meshes[ii] : in.host_dyn[ii - n_static];
        uint32_t cls = ci.cls;
        if (ii >= n_static && !w->host_class_map.empty())  // (a world section's level: the session's classes it has, at their ties here)
            cls = ci.cls < w->host_class_map.size() && w->host_class_map[ci.cls] >= 0 ? static_cast<uint32_t>(w->host_class_map[ci.cls]) : 0xffffffffu;
        if (cls >= w->ties.size()) continue;
        if (ii < n_static && ((in.host_sections >> (ci.section & 31u)) & 1u) == 0) continue;  // (another section's than the current: not drawn)
        const TieClass& c = w->ties[cls];
        float sph[4];
        std::memcpy(sph, c.header.data() + 0x30, sizeof sph);
        const float sc = c.scale * ci.scale;
        float centre[3], rad = sph[3] * sc;
        for (int l = 0; l < 3; ++l) centre[l] = ci.pos[l] + sc * (ci.rot[0][l] * sph[0] + ci.rot[1][l] * sph[1] + ci.rot[2][l] * sph[2]);
        float d2 = 0;
        for (int r = 0; r < 3; ++r) d2 += (centre[r] - in.camera[static_cast<size_t>(r)]) * (centre[r] - in.camera[static_cast<size_t>(r)]);
        const float zv = std::sqrt(d2);  // (the distance: the GPU clips what is behind the view)
        if (far_cull > 0.0f && zv - rad > far_cull) continue;
        SceneInst x;
        float m[3][3], p[3];
        for (int r = 0; r < 3; ++r)
            for (int l = 0; l < 3; ++l) m[r][l] = ci.rot[r][l] * sc;
        for (int l = 0; l < 3; ++l) p[l] = (ci.pos[l] - in.camera[static_cast<size_t>(l)]) * 1024.0f;
        for (int l = 0; l < 4; ++l) {
            for (int r = 0; r < 3; ++r) {
                float acc = A[0 + l] * m[r][0];
                acc = acc + A[4 + l] * m[r][1];
                x.rows[r][l] = acc + A[8 + l] * m[r][2];
            }
            float acc = A[0 + l] * p[0];
            acc = acc + A[4 + l] * p[1];
            acc = acc + A[8 + l] * p[2];
            x.rows[3][l] = acc + A[12 + l];
        }
        x.morph[2] = 256.0f;  // (the near path: 0x43800000 in z)
        float fog = tie_fog[1] + std::max(0.0f, zv) * tie_fog[0];
        fog = std::min(fog, tie_fog[2]);
        fog = std::max(tie_fog[3], fog);
        x.fog = tie_fog[0] == 0.0f && tie_fog[2] == 0.0f ? 0xffu : (static_cast<uint32_t>(ftoi4(fog)) >> 4) & 0xff;
        x.colour_first = static_cast<uint32_t>(b.f.colours.size());
        b.f.colours.resize(b.f.colours.size() + 64);
        for (uint32_t k = 0; k < 64; ++k)
            for (int l = 0; l < 4; ++l)  // (2^23 + byte, as STMOD 1 leaves them; its palette's entries, then its one colour)
                b.f.colours[x.colour_first + k][static_cast<size_t>(l)] = 0x4b000000u + (k < ci.palette.size() ? ci.palette[k][static_cast<size_t>(l)] : ci.rgba[l]);
        const uint32_t inst = static_cast<uint32_t>(b.f.insts.size());
        b.f.insts.push_back(x);
        for (const TiePacket& pk : c.packets) {
            if (pk.lod != 0) continue;
            b.push(Kicked{}, Program::TieA, 0x000, view, 1);
            ScenePacket& sp = b.f.packets.back();
            sp.tie = &pk;
            sp.inst = inst;
            sp.tex = static_cast<uint32_t>(b.f.tex.size());
            sp.ntex = static_cast<uint32_t>(5 * pk.prims.size());
            for (const int32_t t : pk.prims) {
                const TexturePrim& tp = c.textures[static_cast<size_t>(t)];
                for (uint32_t q = 0; q < 5; ++q)
                    b.f.tex.push_back({static_cast<uint32_t>(tp.ad[q][0]), static_cast<uint32_t>(tp.ad[q][0] >> 32), static_cast<uint32_t>(tp.ad[q][1]), 0u});
            }
            ++b.f.draws[static_cast<int>(Program::TieA)];
        }
    }
}

// orig: (host) the check (RAC1_MRPW_STATIC_CHECK=1): the walker's tie / shrub instance draws against the host's, by (colour
// slot = instance, packet): found / missing / extra, and their morph, F, alpha (exact) and rows (relative) compared
// (the check compares packets by their content: the asset holds the same packet in several classes / LODs, the walker's index
// takes the first it finds)
uint64_t pk_id(int kind, const void* pk) {
    static std::unordered_map<const void*, uint64_t> ids;
    const auto it = ids.find(pk);
    if (it != ids.end()) return it->second;
    const std::vector<uint8_t>& d = kind == 0 ? static_cast<const TiePacket*>(pk)->data : static_cast<const ShrubPacket*>(pk)->vif;
    uint64_t h = 1469598103934665603ull;
    for (uint8_t c : d) h = (h ^ c) * 1099511628211ull;
    ids[pk] = h;
    return h;
}

void static_check(Build& b) {
    const StaticFrame* sf = b.in.stat;
    if (sf == nullptr || !b.in.stat_check) return;
    for (int kind = 0; kind < 2; ++kind) {
        int notes = 0;
        std::unordered_map<uint64_t, uint32_t> mine;
        for (const StaticDraw& d : sf->draws[kind]) {
            const void* pk = kind == 0 ? static_cast<const void*>(d.pk->tie) : static_cast<const void*>(d.pk->shrub);
            mine[pk_id(kind, pk) * 1000003u ^ sf->insts[d.inst].slot] = d.inst;
        }
        size_t used = 0;
        for (const Build::ChkRec& r : b.chk[kind]) {
            ++b.f.chk_walker[kind];
            const auto it = mine.find(pk_id(kind, r.pk) * 1000003u ^ r.slot);
            if (it == mine.end()) {
                ++b.f.chk_missing[kind];
                if (notes++ < 3) b.f.chk_notes.push_back((kind == 0 ? "tie" : "shrub") + std::string(" slot ") + std::to_string(r.slot) + " not made by the host");
                continue;
            }
            ++used;
            ++b.f.chk_found[kind];
            const StaticInst& si = sf->insts[it->second];
            if (kind == 0) {
                if (std::memcmp(si.morph, r.morph, sizeof r.morph) != 0) {
                    ++b.f.chk_morph_bad;
                    if (notes++ < 3) {
                        char t[200];
                        std::snprintf(t, sizeof t, "tie slot %u morph host %.9g %g %.9g %.9g walker %.9g %g %.9g %.9g", r.slot, si.morph[0], si.morph[1], si.morph[2], si.morph[3], r.morph[0],
                                      r.morph[1], r.morph[2], r.morph[3]);
                        b.f.chk_notes.push_back(t);
                    }
                }
                if (si.fog != r.fog) ++b.f.chk_fog_bad;
            } else if (si.alpha != r.alpha) {
                ++b.f.chk_alpha_bad;
                if (notes++ < 3) b.f.chk_notes.push_back("shrub slot " + std::to_string(r.slot) + " alpha host " + std::to_string(si.alpha) + " walker " + std::to_string(r.alpha));
            }
            if (!r.gpu) continue;  // (a clipping program's rows are the clip projection's)
            double worst = 0;
            for (int q = 0; q < 4; ++q)
                for (int l = 0; l < 4; ++l) {
                    const double a = si.rows[q][l], w = r.rows[q][l];
                    const double rel = std::fabs(a - w) / std::max(1.0, std::fabs(w));
                    worst = std::max(worst, rel);
                }
            b.f.chk_rows_maxrel = std::max(b.f.chk_rows_maxrel, worst);
            if (worst > 1e-4) {
                ++b.f.chk_rows_bad;
                if (notes++ < 3) {
                    char t[240];
                    std::snprintf(t, sizeof t, "%s slot %u rows rel %.3g: T host %.6g %.6g %.6g %.6g walker %.6g %.6g %.6g %.6g", kind == 0 ? "tie" : "shrub", r.slot, worst, si.rows[3][0], si.rows[3][1],
                                  si.rows[3][2], si.rows[3][3], r.rows[3][0], r.rows[3][1], r.rows[3][2], r.rows[3][3]);
                    b.f.chk_notes.push_back(t);
                }
            }
        }
        b.f.chk_extra[kind] += static_cast<uint32_t>(sf->draws[kind].size() - std::min(sf->draws[kind].size(), used));
    }
    // the shrub far variants' stream: the host's word for word against the chain's (the shrub range's DIRECT data also holds the
    // texture uploads before it and the TEST_1 writes after it: from the stream's header (its three qwords) on, its length)
    if (sf->on[1] && sf->variant_gif.size() >= 3) {
        ++b.f.chk_gif_frames;
        b.f.chk_gif_quads += sf->variants;
        const auto& H = sf->variant_gif;
        size_t at = 0;
        while (at + 3 <= b.chk_gif.size() && !(b.chk_gif[at] == H[0] && b.chk_gif[at + 1] == H[1] && b.chk_gif[at + 2] == H[2])) ++at;
        const std::vector<std::array<uint32_t, 4>> chain(b.chk_gif.begin() + static_cast<std::ptrdiff_t>(std::min(at, b.chk_gif.size())),
                                                         b.chk_gif.begin() + static_cast<std::ptrdiff_t>(std::min(at + H.size(), b.chk_gif.size())));
        const size_t n = std::min(chain.size(), H.size());
        size_t q = 0;
        while (q < n && chain[q] == H[q]) ++q;
        if (q != chain.size() || q != H.size()) {
            ++b.f.chk_gif_bad;
            if (b.f.chk_gif_bad <= 3) {
                char t[300];
                const auto w = q < chain.size() ? chain[q] : std::array<uint32_t, 4>{};
                const auto h = q < H.size() ? H[q] : std::array<uint32_t, 4>{};
                std::snprintf(t, sizeof t, "far variants' stream: qwords chain %zu host %zu, first difference at %zu: chain %08x %08x %08x %08x host %08x %08x %08x %08x", chain.size(),
                              H.size(), q, w[0], w[1], w[2], w[3], h[0], h[1], h[2], h[3]);
                b.f.chk_notes.push_back(t);
            }
        }
    }
}

// orig: (host) a tfrag draw for the GPU path (mrp_tfrag_gpu.h): the entry, its image / start, the view (VU1 base + 5..8)
// and the pass's constants (0x290..0x29d) as VU1 memory holds them; false when the entry has no plan for it (the CPU reference then)
bool tfrag_gpu_draw(Build& b, const uint8_t* mem, uint32_t base, Program prog, uint32_t start, int cls, int lod) {
    if (b.in.tfrag_check && b.in.tfrag_host != nullptr) {  // (the check: the walker's draws recorded by the entry's EE index)
        const TfragLevel& L0 = *b.in.tfrag_level;
        if (cls >= 0 && static_cast<size_t>(cls) < L0.entries.size() && L0.entries[static_cast<size_t>(cls)].ok)
            b.tchk.push_back({(L0.entries[static_cast<size_t>(cls)].ee - L0.table) / 0x40, static_cast<uint8_t>(prog == Program::TfragB ? 2 : start)});
    }
    if (b.tfrag_discard) return true;  // (made by the host: emit_tfrag_host)
    const TfragLevel& L = *b.in.tfrag_level;
    if (cls < 0 || static_cast<size_t>(cls) >= L.entries.size()) return false;
    const TfragEntryGpu& e = L.entries[static_cast<size_t>(cls)];
    const bool isb = prog == Program::TfragB;
    const TfragPlan* pl = e.ok ? e.plan(lod, start, isb) : nullptr;
    if (pl == nullptr) return false;
    TfragFrame& T = b.f.tfrag;
    std::array<float, 16> view;
    for (uint32_t r = 0; r < 4; ++r)
        for (int l = 0; l < 4; ++l) view[4 * r + static_cast<uint32_t>(l)] = f32(mem, base + 5 + r, l);
    std::array<uint32_t, 4 * 14> consts;
    for (uint32_t c = 0; c < 14; ++c)
        for (int l = 0; l < 4; ++l) consts[4 * c + static_cast<uint32_t>(l)] = w32(mem, 0x290 + c, l);
    {  // the GIF tags the plans assume (the renderer's replay): 0x292 the strips' (PACKED, PRE: PRIM, ST / RGBAQ / XYZF2), 0x294 the texture's (5 A+D)
        const uint64_t s0 = static_cast<uint64_t>(consts[8]) | static_cast<uint64_t>(consts[9]) << 32, s1 = static_cast<uint64_t>(consts[10]);
        const uint64_t t0 = static_cast<uint64_t>(consts[16]) | static_cast<uint64_t>(consts[17]) << 32, t1 = static_cast<uint64_t>(consts[18]);
        const bool strip_ok = ((s0 >> 58) & 3) == 0 && ((s0 >> 46) & 1) == 1 && ((s0 >> 60) & 0xf) == 3 && (s1 & 0xfff) == 0x412;
        const bool tex_ok = ((t0 >> 58) & 3) == 0 && ((t0 >> 46) & 1) == 0 && ((t0 >> 60) & 0xf) == 1 && (t1 & 0xf) == 0xe && (t0 & 0x7fff) == 5;
        if (!strip_ok || !tex_ok) return false;
    }
    if (T.views.empty() || T.views.back() != view) T.views.push_back(view);
    if (T.consts.empty() || T.consts.back() != consts) T.consts.push_back(consts);
    TfragDraw d;
    d.entry = static_cast<uint32_t>(cls);
    d.lod = static_cast<uint8_t>(lod);
    d.code = static_cast<uint8_t>(start);
    d.b = isb;
    d.view = static_cast<uint32_t>(T.views.size() - 1);
    d.consts = static_cast<uint32_t>(T.consts.size() - 1);
    static const bool selfcheck = std::getenv("RAC1_MRPW_TFRAG_SELFCHECK") != nullptr;
    if (selfcheck && b.in.tfrag_colours != nullptr && !b.tfrag_emitted) {
        const TfragColours& C = *b.in.tfrag_colours;
        const uint32_t* col = e.colour_slot + e.colour_n <= C.mirror.size() ? C.mirror.data() + e.colour_slot : nullptr;
        b.f.tfrag_bad += tfrag_selfcheck(e, *pl, lod, start, isb, col, view.data(), consts.data(), b.f.tfrag_checked, mem, base);
    }
    ScenePacket p;
    if (b.in.tfrag_kicks && !b.tfrag_emitted) {  // (the verification: the reference's kicks, one after the other)
        std::vector<Kicked> ks;
        if (isb) tfrag_b_kick(mem, base, ks, true);
        else tfrag_kick(mem, base, start, ks, true);
        p.k.want_hp = true;
        for (Kicked& k : ks) {
            p.k.q.insert(p.k.q.end(), k.q.begin(), k.q.end());
            p.k.part.insert(p.k.part.end(), k.part.begin(), k.part.end());
            p.k.clip.insert(p.k.clip.end(), k.clip.begin(), k.clip.end());
            p.k.hp.insert(p.k.hp.end(), k.hp.begin(), k.hp.end());
        }
    }
    p.prog = prog;
    p.start = static_cast<uint16_t>(start);
    p.seq = b.f.draw_starts;
    p.view = b.last_tfrag_view;
    p.table = 0;
    for (int l = 0; l < 3; ++l) {  // (the screen formula, as draw_tfrag's packets: X = P.x C0.x / P.w + C5.x)
        p.scr_scale[l] = 1.0f;
        p.scr_off[l] = f32(mem, 0x295, l);
    }
    p.scr_k = 1;
    p.tfrag = static_cast<int32_t>(T.draws.size());
    T.draws.push_back(d);
    b.f.packets.push_back(std::move(p));
    return true;
}

// orig: (host) the frame's tfrag draws the host decided (LOD > 0 / widened: mrp_tfrag_gpu.h tfrag_decide), at the end of
// the tfrag pass's range: each with the view and constants the pass's chain left in VU1 memory (the view at 5..8 and 0x14d..: the pass
// unpacks the same matrix to both buffers; the constants 0x290..); with RAC1_MRPW_TFRAG_CHECK the walker's draws compared with them
void emit_tfrag_host(Build& b, VUState& st) {
    if (b.tfrag_emitted || b.in.tfrag_host == nullptr || b.in.tfrag_level == nullptr) return;
    b.tfrag_emitted = true;
    const TfragLevel& L = *b.in.tfrag_level;
    const auto& host = *b.in.tfrag_host;
    if (b.in.tfrag_check) {
        size_t j = 0;
        for (const auto& w : b.tchk) {
            ++b.f.tchk_walker;
            size_t k = j;
            while (k < host.size() && host[k].first != w.first) ++k;
            if (k == host.size()) {
                ++b.f.tchk_missing;
                continue;
            }
            b.f.tchk_extra += static_cast<uint32_t>(k - j);
            ++b.f.tchk_found;
            if (host[k].second != w.second && !(w.second == 0x0c && host[k].second == 0x06)) {
                ++b.f.tchk_code;
                static int shown = 0;
                if (shown++ < 10) std::printf("[mrpw tfrag] check: entry %u host code %x, walker %x\n", w.first, host[k].second, w.second);
            }
            j = k + 1;
        }
        b.f.tchk_extra += static_cast<uint32_t>(host.size() - std::min(host.size(), j));
        return;
    }
    if (!b.tfrag_discard) return;
    uint8_t* mem = st.mem.data();
    std::array<double, 16> v = view_of_tfrag(mem, 0);
    if (b.last_tfrag_view < 0 || v != b.last_tfrag) {
        b.last_tfrag = v;
        b.last_tfrag_view = static_cast<int>(b.f.views.size());
        b.f.views.push_back(v);
    }
    const bool was = b.tfrag_discard;
    b.tfrag_discard = false;
    for (const auto& [k, code] : host) {
        if (k >= L.by_ee.size() || L.by_ee[k] < 0) continue;
        const bool isb = code == 2;
        const int lod = isb || code >= 0x0e ? 2 : code >= 0x08 ? 1 : 0;
        if (tfrag_gpu_draw(b, mem, 0, isb ? Program::TfragB : Program::TfragA, isb ? 0x002 : code, L.by_ee[k], lod)) {
            ++b.f.tfrag_host;
        }
    }
    b.tfrag_discard = was;
}

// orig: (host) the modern lighting's tfrag casters outside the view (tfrag_shadow_decide), at the end of the tfrag pass's range
// as emit_tfrag_host's draws (the view and constants the pass left in VU1 memory), their packets marked ScenePacket::shadow
void emit_tfrag_shadow(Build& b, VUState& st) {
    if (b.tfrag_shadow_done || b.in.tfrag_shadow == nullptr || b.in.tfrag_level == nullptr || b.in.tfrag_check) return;
    b.tfrag_shadow_done = true;
    const TfragLevel& L = *b.in.tfrag_level;
    uint8_t* mem = st.mem.data();
    std::array<double, 16> v = view_of_tfrag(mem, 0);
    if (b.last_tfrag_view < 0 || v != b.last_tfrag) {
        b.last_tfrag = v;
        b.last_tfrag_view = static_cast<int>(b.f.views.size());
        b.f.views.push_back(v);
    }
    const bool was_discard = b.tfrag_discard, was_emitted = b.tfrag_emitted;
    b.tfrag_discard = false;
    b.tfrag_emitted = true;  // (no kicks / self-check for these)
    const auto& list = *b.in.tfrag_shadow;
    for (size_t n = 0; n < list.size(); ++n) {
        const auto& [k, code] = list[n];
        if (k >= L.by_ee.size() || L.by_ee[k] < 0) continue;
        const int lod = code >= 0x0e ? 2 : code >= 0x08 ? 1 : 0;
        if (tfrag_gpu_draw(b, mem, 0, Program::TfragA, code, L.by_ee[k], lod)) {
            b.f.packets.back().shadow = true;
            b.f.packets.back().shadow_views = b.in.tfrag_shadow_views != nullptr && n < b.in.tfrag_shadow_views->size() ? (*b.in.tfrag_shadow_views)[n] : 0xffff;
        }
    }
    b.tfrag_discard = was_discard;
    b.tfrag_emitted = was_emitted;
    static const bool trace = std::getenv("RAC1_SHADOW_TFRAG_TRACE") != nullptr;
    if (trace) {
        size_t n = 0;
        for (const ScenePacket& p : b.f.packets) n += p.shadow ? 1 : 0;
        std::printf("[tfrag-shadow] decided %zu emitted %zu\n", b.in.tfrag_shadow->size(), n);
    }
}

// orig: (host) the direct world's tfrags without the GPU path (the CPU tfrag fallback: SceneInput::tfrag_cpu): the frame's draws the host
// decided (tfrag_decide), each by the CPU reference on the VU1 memory its chain would have left - the entry's image for its LOD from the
// level's data, the view (5..8) and constants (0x290..) the pass's chain left, the entry's lit colours from the resident mirror (as
// tfrag_selfcheck makes it) - pushed as draw_tfrag's CPU path pushes the walker's
void emit_tfrag_cpu(Build& b, const VUState& st) {
    if (b.tfrag_emitted || b.in.tfrag_host == nullptr || b.in.tfrag_cpu == nullptr || b.in.tfrag_level != nullptr) return;
    b.tfrag_emitted = true;
    const TfragLevel& L = *b.in.tfrag_cpu;
    const uint8_t* vm = st.mem.data();
    std::array<double, 16> v = view_of_tfrag(vm, 0);
    if (b.last_tfrag_view < 0 || v != b.last_tfrag) {
        b.last_tfrag = v;
        b.last_tfrag_view = static_cast<int>(b.f.views.size());
        b.f.views.push_back(v);
    }
    const float one[3] = {1, 1, 1}, c5[3] = {f32(vm, 0x295, 0), f32(vm, 0x295, 1), f32(vm, 0x295, 2)};
    thread_local std::vector<uint8_t> mem;
    std::vector<Kicked> ks;
    for (const auto& [k, code] : *b.in.tfrag_host) {
        if (k >= L.by_ee.size() || L.by_ee[k] < 0) continue;
        const TfragEntryGpu& e = L.entries[static_cast<size_t>(L.by_ee[k])];
        if (!e.ok) continue;
        const bool isb = code == 2;
        const int lod = isb || code >= 0x0e ? 2 : code >= 0x08 ? 1 : 0;
        mem.assign(0x4000, 0);
        const std::vector<uint8_t>& img = e.image[static_cast<size_t>(lod)];
        std::memcpy(mem.data(), img.data(), std::min<size_t>(img.size(), 0x290 * 16));
        std::memcpy(mem.data() + 5 * 16, vm + 5 * 16, 64);
        std::memcpy(mem.data() + 0x290 * 16, vm + 0x290 * 16, 14 * 16);
        const TfragColours* C = b.in.tfrag_cpu_colours;
        const uint32_t* col = C != nullptr && e.colour_slot + e.colour_n <= C->mirror.size() ? C->mirror.data() + e.colour_slot : nullptr;
        const uint32_t ca = e.colour_addr[static_cast<size_t>(lod)], cn = e.colour_count[static_cast<size_t>(lod)];
        for (uint32_t c = 0; c < cn; ++c) {
            const uint32_t w = col != nullptr ? col[c] : 0;
            const uint32_t q[4] = {w & 0xff, (w >> 8) & 0xff, (w >> 16) & 0xff, w >> 24};
            std::memcpy(mem.data() + 16 * (ca + 2 * c), q, 16);
        }
        ks.clear();
        if (isb) tfrag_b_kick(mem.data(), 0, ks, true);
        else tfrag_kick(mem.data(), 0, code, ks, true);
        b.scr_scale = one;
        b.scr_off = c5;
        const Program prog = isb ? Program::TfragB : Program::TfragA;
        for (Kicked& kk : ks) b.push(std::move(kk), prog, isb ? 0x002 : code, b.last_tfrag_view, 0);
        b.scr_scale = b.scr_off = nullptr;
        ++b.f.tfrag_host;
        ++b.f.tfrag_cpu;
    }
}

// orig: (host) tfrag A / B's LOD starts: the entry the memory holds at XTOP, its strips by the reference
void draw_tfrag(Build& b, VUState& st, Program prog, uint32_t start) {
    const uint8_t* mem = st.mem.data();
    const int lod = tfrag_lod(prog, start);
    const uint32_t base = st.xtop & 0x3ff;
    WorldIndex::Hit hit;
    // (host, RAC1_PERF_TFRAG_FIND=0: always looked up) a pass the host makes (tfrag_discard, not checked) needs no entry: its draw is
    // tfrag_gpu_draw's early return, the entry only its check's - the lookup (a hash of the entry's image, a map) was a tenth of the
    // MRP scene worker
    static const bool find_always = std::getenv("RAC1_PERF_TFRAG_FIND") != nullptr && std::atoi(std::getenv("RAC1_PERF_TFRAG_FIND")) == 0;
    const bool need_hit = find_always || !b.tfrag_discard || b.in.tfrag_check || b.in.tfrag_level == nullptr;
    if (!need_hit) hit.cls = -1;
    else if (!b.idx.find_tfrag(mem, base, lod, hit)) {
        ++b.f.unknown[static_cast<int>(prog)];
        return;
    }
    std::array<double, 16> v = view_of_tfrag(mem, base);
    if (b.last_tfrag_view < 0 || v != b.last_tfrag) {
        b.last_tfrag = v;
        b.last_tfrag_view = static_cast<int>(b.f.views.size());
        b.f.views.push_back(v);
    }
    if (b.in.tfrag_level != nullptr && tfrag_gpu_draw(b, mem, base, prog, start, hit.cls, lod)) {
        ++b.f.draws[static_cast<int>(prog)];
        return;
    }
    ++b.f.tfrag_cpu;
    std::vector<Kicked> ks;
    if (prog == Program::TfragB) tfrag_b_kick(mem, base, ks, true);
    else tfrag_kick(mem, base, start, ks, true);
    // (the screen: X = P.x C0.x / P.w + C5.x, 0x290 / 0x295)
    const float one[3] = {1, 1, 1}, c5[3] = {f32(mem, 0x295, 0), f32(mem, 0x295, 1), f32(mem, 0x295, 2)};
    b.scr_scale = one;
    b.scr_off = c5;
    for (Kicked& k : ks) b.push(std::move(k), prog, start, b.last_tfrag_view, 0);
    b.scr_scale = b.scr_off = nullptr;
    ++b.f.draws[static_cast<int>(prog)];
}

// orig: (host) one PACKED GIF word to the GS register write it makes, as the replica takes it (gs_emu.cpp Gs::packed): RGBA
// without Q, STQ, UV, XYZF2 / XYZ2 with the ADC bit, FOG, A+D (the sky's streams, a world pass's vertices)
void packed_write(uint32_t d, const uint8_t* q, std::vector<SkyWrite>& out) {
    const uint32_t w0 = r32(q), w1 = r32(q + 4), w2 = r32(q + 8), w3 = r32(q + 12);
    const bool adc = ((w3 >> 15) & 1) != 0;
    switch (d) {
    case 0x0: out.push_back(SkyWrite{w0 & 0x7ffu, 0, 0x00, 0}); break;
    case 0x1: out.push_back(SkyWrite{(w0 & 0xffu) | ((w1 & 0xffu) << 8) | ((w2 & 0xffu) << 16) | (static_cast<uint64_t>(w3 & 0xffu) << 24), 0, 0x01, 1}); break;
    case 0x2: out.push_back(SkyWrite{w0 | (static_cast<uint64_t>(w1) << 32), w2, 0x02, 2}); break;
    case 0x3: out.push_back(SkyWrite{(w0 & 0x3fffu) | ((w1 & 0x3fffu) << 16), 0, 0x03, 0}); break;
    case 0x4: case 0xc:
        out.push_back(SkyWrite{(w0 & 0xffffu) | ((w1 & 0xffffu) << 16) | (static_cast<uint64_t>((w2 >> 4) & 0xffffff) << 32) | (static_cast<uint64_t>((w3 >> 4) & 0xff) << 56), 0,
                               static_cast<uint8_t>(d == 0x4 && !adc ? 0x04 : 0x0c), static_cast<uint8_t>(adc ? 3 : 0)});
        break;
    case 0x5: case 0xd:
        out.push_back(SkyWrite{(w0 & 0xffffu) | ((w1 & 0xffffu) << 16) | (static_cast<uint64_t>(w2) << 32), 0, static_cast<uint8_t>(d == 0x5 && !adc ? 0x05 : 0x0d),
                               static_cast<uint8_t>(adc ? 3 : 0)});
        break;
    case 0xa: out.push_back(SkyWrite{static_cast<uint64_t>((w3 >> 4) & 0xff) << 56, 0, 0x0a, 0}); break;
    case 0xe: out.push_back(SkyWrite{r64(q), 0, q[8], 0}); break;
    case 0xf: break;
    default: out.push_back(SkyWrite{r64(q), 0, static_cast<uint8_t>(d), 0}); break;
    }
}

// orig: (host) the GIF's PATH2 (DIRECT) stream: the registers its PACKED A+D / PRIM words and REGLISTs write (the tag in progress
// kept across the DIRECTs, as the replica's Gs::gif: an IMAGE's data may come in a later transfer). its PACKED vertex
// words (RGBA, STQ, UV, XYZ(F), FOG: the shrub far variants' quads the EE builds) as the GS register writes
// they make (SceneFrame::sky), the run between two register writes a `gif` packet of its own, with the pass's view (`build`) and
// `table`, when `draw` (not when the pass's ties / shrubs are the host's: the host makes its far variants then, emit_variants)
struct GifPath {
    uint32_t left = 0, flg = 0, nreg = 1, reg = 0;
    uint64_t regs = 0;
    bool draw = false;
    Build* build = nullptr;  // (the pass's view: Build::view)
    int table = 0;
    uint32_t open = 0xffffffffu;  // the open packet's first write (SceneFrame::sky)
    // (the direct world's scan: the register writes and the vertices' packets noted as its events, the vertices in its writes)
    DirectWorld* dw = nullptr;
    std::vector<SkyWrite>& out(SceneFrame& f) { return dw != nullptr ? dw->writes : f.sky; }
    void close(SceneFrame& f) {
        if (open == 0xffffffffu) return;
        if (dw != nullptr) {
            if (dw->writes.size() > open) {
                DirectEvent e;
                e.kind = 1;
                e.table = static_cast<uint8_t>(table);
                e.first = open;
                e.count = static_cast<uint32_t>(dw->writes.size()) - open;
                dw->events.push_back(e);
            }
        } else if (f.sky.size() > open) {
            ScenePacket p;
            p.gif = true;
            p.table = table;
            p.view = build != nullptr ? build->view(false) : -1;
            p.seq = 0xffffffffu;  // (not a world draw: the check's grouping ignores it)
            p.sky_first = open;
            p.sky_count = static_cast<uint32_t>(f.sky.size()) - open;
            f.packets.push_back(std::move(p));
            ++f.gif_packets;
        }
        open = 0xffffffffu;
    }
    void reg_write(SceneFrame& f, uint8_t r, uint64_t v) {
        close(f);
        if (dw != nullptr) {
            DirectEvent e;
            e.kind = 0;
            e.reg = r;
            e.value = v;
            dw->events.push_back(e);
            return;
        }
        f.regs.push_back({static_cast<uint32_t>(f.packets.size()), r, v});
    }
    void data(const uint8_t* p, uint32_t qwc, SceneFrame& f) {
        for (uint32_t i = 0; i < qwc; ++i) {
            const uint8_t* q = p + 16 * i;
            if (left != 0 && flg != 0 && flg != 1) {  // (an IMAGE's data skipped whole, not quadword by quadword)
                const uint32_t k = std::min(left, qwc - i);
                left -= k;
                i += k - 1;
                continue;
            }
            if (left == 0) {
                const uint64_t lo = r64(q), hi = r64(q + 8);
                const uint32_t nloop = lo & 0x7fff;
                flg = (lo >> 58) & 3;
                nreg = (lo >> 60) & 0xf;
                if (nreg == 0) nreg = 16;
                regs = hi;
                reg = 0;
                if (flg == 0 && ((lo >> 46) & 1) != 0) reg_write(f, 0x00, (lo >> 47) & 0x7ff);
                left = flg == 2 ? nloop : nloop * nreg;
                continue;
            }
            if (flg == 0) {
                const uint32_t d = static_cast<uint32_t>(regs >> (4 * reg)) & 0xf;
                if (d == 0xe) reg_write(f, q[8], r64(q));
                else if (d == 0x0) reg_write(f, 0x00, r64(q) & 0x7ff);
                else if (draw && d != 0xf) {
                    if (open == 0xffffffffu) open = static_cast<uint32_t>(out(f).size());
                    packed_write(d, q, out(f));
                }
                reg = (reg + 1) % nreg;
                --left;
            } else if (flg == 1) {
                for (int h = 0; h < 2 && left != 0; ++h) {
                    const uint32_t d = static_cast<uint32_t>(regs >> (4 * reg)) & 0xf;
                    if (d != 0xe && d != 0xf) reg_write(f, static_cast<uint8_t>(d), r64(q + 8 * h));
                    reg = (reg + 1) % nreg;
                    --left;
                }
            } else {
                --left;
            }
        }
        close(f);
    }
};

// orig: (host) the host-made shrub far variants' quads (StaticFrame::variant_gif: the shrub pass's DIRECT data as 0x287c04.. writes
// it) drawn as the chain's are (GifPath, the shrub pass's view and table); RAC1_MRPW_GIF=0 not drawn, as the chain's
void emit_variants(Build& b) {
    static const bool no_gif = std::getenv("RAC1_MRPW_GIF") != nullptr && std::atoi(std::getenv("RAC1_MRPW_GIF")) == 0;
    const StaticFrame* sf = b.in.stat;
    if (sf == nullptr || sf->variant_gif.empty() || no_gif) return;
    GifPath g;
    g.draw = true;
    g.build = &b;
    g.table = 2;
    g.data(reinterpret_cast<const uint8_t*>(sf->variant_gif.data()), static_cast<uint32_t>(sf->variant_gif.size()), b.f);
    b.f.stat_variants += sf->variants;
}


// orig: (host) the sky's GIF streams as GS register writes, as the replica takes them (gs_emu.cpp Gs::gif / packed):
// PACKED (its formats converted to the registers': RGBA without Q, STQ, UV, XYZF2 / XYZ2 with the ADC bit, FOG, A+D), REGLIST, an
// IMAGE's data skipped. PATH2 keeps its tag in progress across the DIRECTs (the texture uploads' IMAGE data comes in the REF after
// its tag); a PATH1 kick is a packet of its own.
struct SkyGif {
    uint32_t left = 0, flg = 0, nreg = 1, reg = 0;
    uint64_t regs = 0;
    void data(const uint8_t* p, uint32_t qwc, std::vector<SkyWrite>& out) {
        for (uint32_t i = 0; i < qwc; ++i) {
            const uint8_t* q = p + 16 * i;
            if (left == 0) {
                const uint64_t lo = r64(q), hi = r64(q + 8);
                const uint32_t nloop = lo & 0x7fff;
                flg = (lo >> 58) & 3;
                nreg = (lo >> 60) & 0xf;
                if (nreg == 0) nreg = 16;
                regs = hi;
                reg = 0;
                if (flg == 0 && ((lo >> 46) & 1) != 0) out.push_back(SkyWrite{(lo >> 47) & 0x7ff, 0, 0x00, 0});
                left = flg == 2 || flg == 3 ? nloop : nloop * nreg;
                continue;
            }
            if (flg == 0) {
                packed_write(static_cast<uint32_t>(regs >> (4 * reg)) & 0xf, q, out);
                reg = (reg + 1) % nreg;
                --left;
            } else if (flg == 1) {
                for (int h = 0; h < 2 && left != 0; ++h) {
                    const uint32_t d = static_cast<uint32_t>(regs >> (4 * reg)) & 0xf;
                    if (d != 0xe && d != 0xf) out.push_back(SkyWrite{r64(q + 8 * h), 0, static_cast<uint8_t>(d), 0});
                    reg = (reg + 1) % nreg;
                    --left;
                }
            } else {
                --left;
            }
        }
    }
};

// ---- the direct world (RAC1_PERF_DIRECT_WORLD, mrp_world_scene.h) ---------------------------------------------------------------

// The scan's private VU1 (its own: the walker's is the sky's and the set-up's; persistent as the walker's)
struct ScanWalker {
    vu1::Vu1 vu;
    bool micro_dirty = true;
    Program prog = Program::None;
};
ScanWalker* g_scan = nullptr;
std::atomic<bool> g_direct_forced{false};
DirectStats g_dstats;
SceneFrame g_scan_dummy;  // (GifPath's frame argument: the scan's writes go to its DirectWorld)

// orig: (host) the direct world can make this frame's scene: the host makes the ties, the shrubs and the tfrags (not the checks'
// modes), no world pass ran a shadow chain
bool direct_usable(const SceneInput& in) {
    if (in.stat == nullptr || in.stat_level == nullptr || in.stat_check || !in.stat->on[0] || !in.stat->on[1]) return false;
    if (in.tfrag_host == nullptr || (in.tfrag_level == nullptr && in.tfrag_cpu == nullptr) || in.tfrag_check || in.tfrag_kicks) return false;
    for (const SceneShadow& s : in.shadows)
        if (s.pass >= 0 && s.pass <= 2) return false;
    return true;
}

// the tfrag pass's qwords the host's tfrag draws read (the view at 5..8 of both buffers, the constants 0x290..0x29d): an UNPACK
// writing any of `span` qwords from `d` reaches them
bool tfrag_window(uint32_t d, uint32_t span) {
    if (span >= 0x400) return true;
    auto meets = [](uint32_t lo, uint32_t hi) { return (lo < 9 && hi > 5) || (lo < 0x151 && hi > 0x14d) || (lo < 0x29e && hi > 0x290); };
    d &= 0x3ff;
    if (d + span <= 0x400) return meets(d, d + span);
    return meets(d, 0x400) || meets(0, d + span - 0x400);  // (past the memory's end: from its start again)
}

// orig: (host) the VU1 state a pass's chain left, as the host's draws read it
DirectState direct_state(const VUState& st) {
    DirectState s;
    for (uint32_t q = 0; q < 4; ++q) s.view[q] = q128(st.mem.data(), 5 + q);
    for (uint32_t q = 0; q < 14; ++q) s.consts[q] = q128(st.mem.data(), 0x290 + q);
    for (uint32_t q = 0; q < 3; ++q) s.tie[q] = q128(st.mem.data(), 0x3cb + q);
    s.shrub_fog = {st.vf[30].f[2], st.vf[30].f[3], st.vf[29].f[0], st.vf[29].f[1]};
    return s;
}
// orig: (host) the same put back into a VU1 state (the qwords and registers emit_* read)
void direct_state_put(const DirectState& s, VUState& st) {
    for (uint32_t q = 0; q < 4; ++q) std::memcpy(st.mem.data() + (5 + q) * 16, s.view[q].data(), 16);
    for (uint32_t q = 0; q < 14; ++q) std::memcpy(st.mem.data() + (0x290 + q) * 16, s.consts[q].data(), 16);
    for (uint32_t q = 0; q < 3; ++q) std::memcpy(st.mem.data() + (0x3cb + q) * 16, s.tie[q].data(), 16);
    st.vf[30].f[2] = s.shrub_fog[0];
    st.vf[30].f[3] = s.shrub_fog[1];
    st.vf[29].f[0] = s.shrub_fog[2];
    st.vf[29].f[1] = s.shrub_fog[3];
}

// orig: (host) the direct world's scan (mrp_world_scene.h): the world's ranges from their first tag `addr` (the capture's walk there,
// its call `stack`) to the walk's end, as scene_build walks them when the host makes every draw - each pass's tie / shrub ranges with
// only their constants (the skipped passes' rule), the tfrag range with only the UNPACKs that reach the view / constants and its
// set-up starts, every DIRECT's register writes - but in place in EE memory (nothing copied) and the draws' data not unpacked.
// false: a tag or its data not in memory (the capture walks the whole chain instead)
bool scan_world(SceneInput& in, uint32_t addr, std::vector<uint32_t>& stack, uint32_t last_end, bool reset) {
    DirectWorld& dw = in.dw;
    dw.ok = false;
    dw.start = addr;
    dw.events.clear();
    dw.writes.clear();
    dw.states.clear();
    dw.tags = dw.codes = dw.unpacks = dw.starts = 0;
    static bool reset_pending = false;  // (a new level asked while no scan ran: the next scan's)
    reset_pending = reset_pending || reset;
    if (g_scan == nullptr || reset_pending) {
        reset_pending = false;
        delete g_scan;
        g_scan = new ScanWalker();
        g_scan->vu.st.vf[0].f[3] = 1.0f;
    }
    ScanWalker& S = *g_scan;
    vu1::Vu1& vu = S.vu;
    auto pass_of = [&in](uint32_t a) {
        for (const PassRange& r : in.passes)
            if (a >= r.first && a < r.end) return r.pass;
        return -1;
    };
    static const bool no_gif = std::getenv("RAC1_MRPW_GIF") != nullptr && std::atoi(std::getenv("RAC1_MRPW_GIF")) == 0;
    GifPath gif;
    gif.dw = &dw;
    uint32_t draw_starts = 0;
    bool emitted[3] = {};
    auto emit = [&](int pass) {  // (where the walk leaves a pass's range: its host draws with the state its chain left)
        if (pass < 0 || pass > 2 || emitted[pass]) return;
        emitted[pass] = true;
        DirectEvent e;
        e.kind = static_cast<uint8_t>(2 + pass);
        e.state = static_cast<uint32_t>(dw.states.size());
        e.draw_starts = draw_starts;
        dw.states.push_back(direct_state(vu.st));
        dw.events.push_back(e);
    };
    auto on_start = [&](uint32_t start, bool world) {
        vu.top = vu.tops;
        vu.itop = vu.itops;
        vu.dbf = !vu.dbf;
        vu.tops = (vu.base + (vu.dbf ? vu.ofst : 0)) & 0x3ff;
        vu.st.xtop = vu.top;
        if (!world) return;
        if (S.micro_dirty) {
            S.prog = program_of(vu.micro.data());
            S.micro_dirty = false;
        }
        if (S.prog == Program::None) return;
        if (!is_draw_start(S.prog, start)) {
            ++dw.starts;
            run_program(vu.st, S.prog, start);
            return;
        }
        ++draw_starts;  // (the walk's draws: decoded but not drawn, the host's drawn)
    };
    int pass_prev = -1;
    std::vector<uint8_t> span;
    bool ok = true;
    for (int guard = 0; guard < 400000; ++guard) {
        if (addr == (in.chain_end & ee_dma_addr_mask()) || addr == last_end) break;
        const uint8_t* t = static_cast<const uint8_t*>(ee_ram_ptr_read(addr));
        if (t == nullptr) {
            ok = false;
            break;
        }
        ++dw.tags;
        const uint64_t lo = r64(t);
        const uint32_t qwc = lo & 0xffff, id = (lo >> 28) & 7;
        const uint32_t a = static_cast<uint32_t>(lo >> 32) & ee_dma_addr_mask();
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
        uint8_t hdr[16] = {};
        std::memcpy(hdr + 8, t + 8, 8);
        const uint8_t* dp = hdr + 16;
        if (qwc != 0) {
            dp = static_cast<const uint8_t*>(ee_ram_ptr_read(data));
            if (dp == nullptr || ee_ram_ptr_read(data + qwc * 16 - 1) == nullptr) {
                ok = false;
                break;
            }
        }
        const uint32_t n = 16 + qwc * 16;
        auto at = [&hdr, &dp, &span](uint32_t i, uint32_t len) -> const uint8_t* {
            if (i >= 16) return dp + (i - 16);
            if (i + len <= 16) return hdr + i;
            span.assign(hdr + i, hdr + 16);
            span.insert(span.end(), dp, dp + (i + len - 16));
            return span.data();
        };
        const int pass_now = pass_of(addr);
        if (pass_prev != pass_now) emit(pass_prev);
        pass_prev = pass_now;
        const bool world = pass_now >= 0 && pass_now <= 2;
        const bool skip = pass_now == 1 || pass_now == 2;  // (the host makes the ties / shrubs: their constants alone)
        for (uint32_t i = 0; i + 4 <= n;) {
            const uint32_t code = i >= 16 ? r32(dp + (i - 16)) : r32(at(i, 4));
            i += 4;
            ++dw.codes;
            const uint32_t cmd = (code >> 24) & 0x7f, imm = code & 0xffff;
            switch (cmd) {  // (the VIF's state codes without data, as vif1_command sets them: the common case, inline)
            case 0x00: case 0x10: case 0x11: case 0x13: continue;  // NOP, FLUSHE, FLUSH, FLUSHA
            case 0x01: vu.cl = imm & 0xff; vu.wl = (imm >> 8) & 0xff; continue;
            case 0x02: vu.ofst = imm & 0x3ff; vu.dbf = false; vu.tops = vu.base; continue;
            case 0x03: vu.base = imm & 0x3ff; continue;
            case 0x04: vu.itops = imm & 0x3ff; continue;
            case 0x05: vu.mode = imm & 3; continue;
            default: break;
            }
            if (cmd == 0x50 || cmd == 0x51) {  // DIRECT / DIRECTHL: the GS registers it writes (and a tfrag range's vertices, none so far)
                const uint32_t q = imm == 0 ? 65536 : imm;
                i = (i + 15) & ~15u;
                const uint32_t take = std::min(q * 16, n - std::min(n, i));
                gif.draw = world && pass_now == 0 && !no_gif;
                gif.table = pass_now;
                gif.data(at(i, take), take / 16, g_scan_dummy);
                i += q * 16;
                continue;
            }
            const uint32_t nb = vu1::vif_data_bytes(vu, code);
            if (nb != 0 && i + nb > n) break;
            auto constants_start = [&](uint32_t start) { return pass_now == 1 ? (start & 0x3ff) == 0x008 : (start & 0x3ff) == 0x000; };
            const uint32_t dest = ((imm & 0x3ff) + ((imm & 0x8000) != 0 ? vu.tops : 0)) & 0x3ff;
            auto constants_dest = [&]() { return pass_now == 1 ? dest >= 0x3c6 && dest < 0x3d0 : dest < 6; };
            if (cmd == 0x14 || cmd == 0x15) {
                on_start(imm, world && (!skip || constants_start(imm)));
                vu.next_pc = imm;
            } else if (cmd == 0x17) {
                on_start(vu.next_pc, world && (!skip || constants_start(vu.next_pc)));
            } else if (cmd == 0x4a) {  // MPG: the world's only
                if (world) {
                    S.micro_dirty = true;
                    vu1::vif1_command(vu, code, at(i, nb), nullptr);
                }
            } else if (cmd >= 0x60) {  // UNPACK: the world's, the constants' only
                const uint32_t num = ((code >> 16) & 0xff) == 0 ? 256 : (code >> 16) & 0xff;
                const bool take = world && (skip ? constants_dest() : tfrag_window(dest, num * std::max({1u, vu.cl, vu.wl}) + 4));
                if (take) {
                    ++dw.unpacks;
                    vu1::vif1_command(vu, code, at(i, nb), nullptr);
                }
            } else {
                vu1::vif1_command(vu, code, at(i, nb), nullptr);
            }
            i += nb;
        }
        if (stop) break;
        addr = next;
    }
    if (!ok) return false;
    gif.close(g_scan_dummy);
    emit(0);  // (the passes the walk ended in or never met: their host draws at its end, as scene_build's)
    emit(1);
    emit(2);
    dw.draw_starts = draw_starts;
    dw.ok = true;
    return true;
}

}  // namespace

// orig: (host)
bool is_draw_start(Program prog, uint32_t start) {
    switch (prog) {
    case Program::ShrubA: case Program::ShrubB: return start == 0x063;
    case Program::TieA: case Program::TieB: return start == 0x000;
    case Program::TfragA: case Program::TfragB: return tfrag_lod(prog, start) >= 0;
    default: return false;
    }
}

// orig: (host)
void scene_begin_update() {
    g_passes.clear();
    g_shadows.clear();
    world_chained_clear();
}

// orig: (host)
void scene_note_pass(int pass, uint32_t first, uint32_t end) {
    if (end > first) g_passes.push_back({pass, first, end});
}

// orig: (host)
const std::vector<PassRange>& scene_passes() { return g_passes; }

// orig: (host)
void scene_note_shadow(int pass, uint32_t first, uint32_t end, uint32_t base, std::vector<uint8_t>&& bytes, std::vector<std::pair<uint64_t, uint32_t>>&& sky_tex,
                       const std::array<float, 16>* sky_a) {
    if (end <= first || bytes.empty()) return;
    Shadow s{pass, first, end, base, std::move(bytes), std::move(sky_tex)};
    if (sky_a != nullptr) {
        s.has_a = true;
        s.sky_a = *sky_a;
    }
    g_shadows.push_back(std::move(s));
}

// orig: (host)
void sky_texture_entries(std::vector<std::pair<uint64_t, uint32_t>>& out) { sky_texture_entries_at(out, OV(0x0016051cu)); }

// orig: (host) the same from the sky asset's pointer at `asset_ptr` (0x16051c on the level's overlay, translated by the caller)
void sky_texture_entries_at(std::vector<std::pair<uint64_t, uint32_t>>& out, uint32_t asset_ptr) {
    out.clear();
    const uint32_t asset = lwu(asset_ptr);
    if (asset == 0) return;
    const int32_t n = static_cast<int16_t>(lhu(asset + 0xc));
    const uint32_t e0 = lwu(asset + 0x10);
    for (int32_t k = 0; k < n && k < 256; ++k) {
        const uint32_t e = e0 + static_cast<uint32_t>(k) * 0x10;
        const uint64_t t = static_cast<uint64_t>(lwu(e)) | (static_cast<uint64_t>(lwu(e + 4)) << 32);
        if (t != 0) out.push_back({t, e});
    }
}

// orig: (host) the chained world's capture (the game's state frozen): the passes' notes of this frame's chain, each tie draw's
// 0x60 bytes and each shrub instance's 0x50 bytes of the frame packet, and a shrub instance's alpha as shrub A reads it (the w of
// its first colour: the colours' fourth byte, or - a faded one - the fade its matrix's REF brings at +0x4c). A run with an address
// outside the RAM does not stand
namespace {
void chained_capture(ChainedWorld& c) {
    c.notes = world_chained();
    c.tie_bytes.resize(0x60 * c.notes.tie.size());
    c.shrub_first.resize(c.notes.shrub.size());
    c.shrub_bytes.clear();
    c.shrub_alpha.clear();
    c.tie_tail.resize(c.notes.runs.size());
    auto bytes = [](uint32_t a, uint32_t n) -> const uint8_t* {
        return ee_is_valid_addr(a, n) ? static_cast<const uint8_t*>(ee_ram_ptr_read(a)) : nullptr;
    };
    for (WorldChained::Run& r : c.notes.runs) {
        if (!r.ok) continue;
        if (r.kind == 1) {
            for (uint32_t i = r.at; r.ok && i < r.at + r.n; ++i) {
                const uint8_t* p = bytes(c.notes.tie[i].matrix, 0x60);
                if (p == nullptr) r.ok = false;
                else std::memcpy(c.tie_bytes.data() + 0x60 * static_cast<size_t>(i), p, 0x60);
            }
            for (uint32_t k = 0; r.ok && k < 2 && k < r.n; ++k) {  // (its last two draws' colours: chained_tables)
                const uint8_t* p = bytes(c.notes.tie[r.at + r.n - 1 - k].colours, 256);
                if (p == nullptr) r.ok = false;
                else std::memcpy(c.tie_tail[static_cast<size_t>(&r - c.notes.runs.data())].data() + 256 * k, p, 256);
            }
        } else if (r.kind == 2) {
            for (uint32_t i = r.at; i < r.at + r.n; ++i) {
                const WorldChained::Shrub& g = c.notes.shrub[i];
                c.shrub_first[i] = static_cast<uint32_t>(c.shrub_alpha.size());
                for (uint32_t k = 0; k < g.n; ++k) {
                    const uint8_t* p = bytes(g.matrix[k], 0x50);
                    const uint8_t* col = bytes(g.colours[k], 4);
                    const size_t at = c.shrub_bytes.size();
                    c.shrub_bytes.resize(at + 0x50);
                    if (p == nullptr || col == nullptr) {
                        r.ok = false;
                        c.shrub_alpha.push_back(0);
                        continue;
                    }
                    std::memcpy(c.shrub_bytes.data() + at, p, 0x50);
                    c.shrub_alpha.push_back(g.faded ? static_cast<uint8_t>(r32(p + 0x4c) & 0xff) : col[3]);
                }
            }
        }
    }
}
}  // namespace

// orig: (host) see mrp_world_scene.h: the walk's EE reads in the order scene_build makes them, copied
void scene_capture(uint32_t chain_start, uint32_t chain_end, SceneInput& in) {
    const SceneAddrs ad = scene_addrs();
    scene_capture(chain_start, chain_end, in, ad);
}

// orig: (host) see mrp_world_scene.h
SceneAddrs scene_addrs() { return SceneAddrs{OV(0x00166d00u), OV(0x00166d40u), OV(0x00166dc0u), OV(0x0016051cu)}; }

// orig: (host) see mrp_world_scene.h
void scene_capture(uint32_t chain_start, uint32_t chain_end, SceneInput& in, const SceneAddrs& ad) {
    in.bytes.clear();
    in.spans.clear();
    in.passes = g_passes;
    in.shadows = g_shadows;  // (copied: a host frame without a game frame - paused, single-stepped - captures the same game frame again)
    in.chain_start = chain_start;
    in.chain_end = chain_end;
    in.ok = !in.passes.empty() && chain_start != 0;
    in.has_sky = false;
    in.sky_tex.clear();
    in.sky_asset = 0;
    in.chained.notes.runs.clear();
    if (!in.ok) return;
    if (in.chained.want) chained_capture(in.chained);
    // the projections A (0x166d00, the clipping programs' 0x166d40) and the camera (0x166dc0) the views are made from
    for (uint32_t k = 0; k < 16; ++k) {
        in.a[k] = lwc1(ad.a + 4 * k);
        in.a_clip[k] = lwc1(ad.a_clip + 4 * k);
    }
    for (uint32_t k = 0; k < 3; ++k) in.camera[k] = lwc1(ad.camera + 4 * k);
    if (host_meshes_bound_any()) {  // (a level mod's host meshes: the tie pass's fog, 0x16cc60, for their draws)
        float fog[4];
        for (uint32_t k = 0; k < 4; ++k) fog[k] = lwc1(OV(0x0016cc60u) + 4 * k);
        host_meshes_fog_set(fog);
        host_meshes_dynamic(in.host_dyn);  // (the level's objects' models: their places this frame)
        in.host_sections = host_meshes_sections();  // (a session's current section: its meshes drawn)
    } else {
        in.host_dyn.clear();
        in.host_sections = 0xffffffffu;
    }
    uint32_t last_end = 0;
    for (const PassRange& r : in.passes) last_end = std::max(last_end, r.end);
    constexpr size_t kSeen = 1u << 15;
    in.seen.assign(kSeen, {0, 0, 0});
    auto take = [&in](uint32_t a, uint32_t len) -> const uint8_t* {
        const uint8_t* src = static_cast<const uint8_t*>(ee_ram_ptr_read(a));
        if (src == nullptr) {
            in.spans.push_back({a, static_cast<uint32_t>(in.bytes.size()), 0xffffffffu});
            return nullptr;
        }
        if (len >= 64) {  // (a range copied before in this capture: the same bytes)
            size_t h = (static_cast<size_t>(a) * 0x9E3779B1u ^ len) & (kSeen - 1);
            for (int probe = 0; probe < 8; ++probe, h = (h + 1) & (kSeen - 1)) {
                auto& e = in.seen[h];
                if (e[2] == 0) {
                    e = {a, len, static_cast<uint32_t>(in.bytes.size()) + 1};
                    break;
                }
                if (e[0] == a && e[1] == len) {
                    in.spans.push_back({a, e[2] - 1, len});
                    return in.bytes.data() + (e[2] - 1);
                }
            }
        }
        const size_t at = in.bytes.size();
        in.spans.push_back({a, static_cast<uint32_t>(at), len});
        in.bytes.resize(at + len);
        std::memcpy(in.bytes.data() + at, src, len);
        return in.bytes.data() + at;
    };
    // the direct world (RAC1_PERF_DIRECT_WORLD): the host makes every world draw and no pass ran a shadow chain - the world's ranges
    // scanned in place where the walk meets them, not copied (a scan that fails: the whole chain copied as before)
    const bool direct = in.direct && direct_usable(in);
    in.dw.ok = false;
    // the walk as scene_build's (the tags followed, the shadow chains' ranges read from their copies)
    std::vector<uint32_t> stack;
    const Shadow* sh = nullptr;
    auto in_sh = [&sh](uint32_t a) { return sh != nullptr && a >= sh->base && a - sh->base < sh->bytes.size(); };
    uint32_t addr = chain_start & ee_dma_addr_mask();
    bool scanned = false;
    for (int guard = 0; guard < 400000; ++guard) {
        if (sh == nullptr) {
            if (addr == (chain_end & ee_dma_addr_mask()) || addr == last_end) break;
            if (direct && !scanned) {
                int pass = -1;
                for (const PassRange& r : in.passes)
                    if (addr >= r.first && addr < r.end) pass = r.pass;
                if (pass >= 0 && pass <= 2) {
                    scanned = true;
                    const auto t0 = std::chrono::steady_clock::now();
                    std::vector<uint32_t> st2 = stack;
                    if (scan_world(in, addr, st2, last_end, in.dw_reset)) {
                        g_dstats.tags += in.dw.tags;
                        g_dstats.codes += in.dw.codes;
                        g_dstats.unpacks += in.dw.unpacks;
                        g_dstats.starts += in.dw.starts;
                        in.dw.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                        g_dstats.ms += in.dw.ms;
                        ++g_dstats.scans;
                        break;
                    }
                    ++g_dstats.fallback;  // (the rest of the chain copied: the walk as before)
                }
            }
            for (const Shadow& sd : in.shadows)
                if (sd.first == addr) {
                    sh = &sd;
                    addr = sd.base;
                }
        } else if (addr < sh->base || addr - sh->base >= sh->bytes.size()) {
            addr = sh->end;
            sh = nullptr;
            continue;
        }
        uint64_t lo = 0;
        if (in_sh(addr)) {
            std::memcpy(&lo, sh->bytes.data() + (addr - sh->base), 8);
        } else {
            const uint8_t* t = take(addr, 16);
            if (t == nullptr) break;
            std::memcpy(&lo, t, 8);
        }
        const uint32_t qwc = lo & 0xffff, id = (lo >> 28) & 7;
        const uint32_t a = static_cast<uint32_t>(lo >> 32) & ee_dma_addr_mask();
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
        if (qwc != 0) {
            if (in_sh(data)) {
                if (data - sh->base + qwc * 16 > sh->bytes.size()) {  // (across the shadow chain's end: quadword by quadword)
                    bool ended = false;
                    for (uint32_t q = 0; q < qwc && !ended; ++q)
                        if (!in_sh(data + q * 16)) ended = take(data + q * 16, 16) == nullptr;
                }
            } else if (take(data, qwc * 16) == nullptr) {
                break;
            }
        }
        if (stop) {
            if (sh == nullptr) break;
            next = sh->end;
            sh = nullptr;
        }
        addr = next;
    }
    static const bool stats = std::getenv("RAC1_MRPW_CAPTURE_STATS") != nullptr;  // (testing: the capture's size)
    if (stats) {
        static uint64_t n = 0, bytes = 0, spans = 0, read = 0;
        ++n;
        bytes += in.bytes.size();
        spans += in.spans.size();
        for (const auto& sp : in.spans) read += sp[2] == 0xffffffffu ? 0 : sp[2];
        if (n % 120 == 0)
            std::printf("[mrpw capture] %.0f KB copied a frame (%.0f KB read), %.0f spans\n", static_cast<double>(bytes) / 1024.0 / static_cast<double>(n),
                        static_cast<double>(read) / 1024.0 / static_cast<double>(n), static_cast<double>(spans) / static_cast<double>(n));
    }
    // the sky: its texture entries' TEX0 this frame (0x28b680 clears them, the drawers fill them), the projection A
    for (const PassRange& r : in.passes)
        if (r.pass == 3) in.has_sky = true;
    if (in.has_sky) {
        in.sky_asset = lwu(ad.sky_asset);
        sky_texture_entries_at(in.sky_tex, ad.sky_asset);
        for (const Shadow& sd : in.shadows)  // (the shadow sky's entries, which the real pass may not have filled)
            if (sd.pass == 3)
                for (const auto& t : sd.sky_tex)
                    if (std::find(in.sky_tex.begin(), in.sky_tex.end(), t) == in.sky_tex.end()) in.sky_tex.push_back(t);
        for (int c = 0; c < 4; ++c)
            for (int l = 0; l < 4; ++l) in.sky_a[static_cast<size_t>(c * 4 + l)] = in.a[static_cast<size_t>(c * 4 + l)];
        for (const Shadow& sd : in.shadows)  // (a widened sky's shadow projected with its own A)
            if (sd.pass == 3 && sd.has_a)
                for (size_t k = 0; k < 16; ++k) in.sky_a[k] = sd.sky_a[k];
    }
}

// orig: (host) see mrp_world_scene.h
uint64_t scene_capture_misses() { return g_capture_misses; }

// orig: (host) see mrp_world_scene.h
int direct_world_mode() {
    static const int m = perf::switch_value("RAC1_PERF_DIRECT_WORLD", 1);
    return m;
}
bool direct_world_on() { return direct_world_mode() == 1 || direct_world_mode() == 3; }
bool direct_world_compare() { return direct_world_mode() == 2 || direct_world_mode() == 4; }
bool direct_world_force() { return direct_world_mode() == 3 || direct_world_mode() == 4; }

// orig: (host) see mrp_world_scene.h
void direct_world_note_forced(bool forced) { g_direct_forced.store(forced, std::memory_order_relaxed); }
bool direct_world_forced() { return g_direct_forced.load(std::memory_order_relaxed); }

// orig: (host) see mrp_world_scene.h
const DirectStats& direct_world_stats() { return g_dstats; }

// orig: (host) see mrp_world_scene.h
int chained_world_mode() {
    static const int m = perf::switch_value("RAC1_PERF_WORLD_CHAINED", 1);
    return m;
}
// orig: (host) see mrp_world_scene.h
const ChainedStats& chained_world_stats() { return g_cstats; }

// orig: (host) see mrp_world_scene.h: the two scenes' packets in order, each field compared - the indices into the frames' tables
// resolved (views, instances and their copied colours, texture qwords, tfrag draws with their view and constants, GIF / sky writes) -
// and the register writes before each packet
namespace {
// orig: (host) a texture prim's A+D qword without the texture's place in VRAM (TEX0's TBP0 / CBP, MIPTBP1 / 2): the chain's is the
// frame's own placement, the level's tables' the level load's, and the world's textures are not found by it (CLAMP's MINV index
// names them)
std::array<uint32_t, 4> tex_placed(std::array<uint32_t, 4> q) {
    const uint32_t r = q[2] & 0xff;
    if (r == 0x34 || r == 0x35) q[0] = q[1] = 0;
    if (r == 0x06 || r == 0x07) {
        q[0] &= ~0x3fffu;
        q[1] &= ~(0x3fffu << 5);
    }
    return q;
}
}  // namespace

bool scene_compare(const SceneFrame& w0, const SceneFrame& d0, bool chained) {
    struct Account {
        perf::DualCheck& check;
        uint64_t &packets, &regs;
    };
    Account st = chained ? Account{g_cstats.check, g_cstats.packets, g_cstats.regs} : Account{g_dstats.check, g_dstats.packets, g_dstats.regs};
    // (the chained world's: the walk names a packet the asset holds in several classes / LODs by the first it finds, the pass's
    // notes by the class and LOD it chained: the same packet when what the chain unpacks for them is the same bytes - a tie
    // packet's VU1 image, a shrub packet's data)
    auto same_packet = [chained](const TiePacket* a, const TiePacket* b, const ShrubPacket* c, const ShrubPacket* d) {
        if (a == b && c == d) return true;
        if (!chained || (a == nullptr) != (b == nullptr) || (c == nullptr) != (d == nullptr)) return false;
        return (a == nullptr || a->image == b->image) && (c == nullptr || c->vif == d->vif);
    };
    // (testing, RAC1_PERF_DIRECT_WORLD_CMP=tfrag: the tfrag packets alone, in order, their `seq` and the registers left out - the CPU tfrag
    // fallback's proof at the game's 4:3 Original, =4, where the host's ties / shrubs are not the walker's)
    static const bool only_tfrag = std::getenv("RAC1_PERF_DIRECT_WORLD_CMP") != nullptr && std::string(std::getenv("RAC1_PERF_DIRECT_WORLD_CMP")) == "tfrag";
    thread_local SceneFrame fw, fd;
    auto tfrags = [](const SceneFrame& src, SceneFrame& dst) {
        dst = src;
        dst.packets.clear();
        dst.regs.clear();
        for (const ScenePacket& p : src.packets)
            if (p.prog == Program::TfragA || p.prog == Program::TfragB) {
                dst.packets.push_back(p);
                dst.packets.back().seq = 0;
            }
    };
    if (only_tfrag) {
        tfrags(w0, fw);
        tfrags(d0, fd);
    }
    const SceneFrame& w = only_tfrag ? fw : w0;
    const SceneFrame& d = only_tfrag ? fd : d0;
    std::string why;
    auto bad = [&why](const std::string& s) {
        if (why.empty()) why = s;
        return false;
    };
    auto same_view = [&](int32_t a, int32_t b) {
        if (a < 0 || b < 0) return a == b;
        if (static_cast<size_t>(a) >= w.views.size() || static_cast<size_t>(b) >= d.views.size()) return false;
        return std::memcmp(w.views[static_cast<size_t>(a)].data(), d.views[static_cast<size_t>(b)].data(), sizeof(double) * 16) == 0;
    };
    auto same_writes = [&](uint32_t fa, uint32_t ca, uint32_t fb, uint32_t cb) {
        if (ca != cb || fa + ca > w.sky.size() || fb + cb > d.sky.size()) return false;
        for (uint32_t k = 0; k < ca; ++k) {
            const SkyWrite &x = w.sky[fa + k], &y = d.sky[fb + k];
            if (x.v != y.v || x.q != y.q || x.reg != y.reg || x.mode != y.mode) return false;
        }
        return true;
    };
    // (the tfrag comparison: the texture A+D qwords' MIPTBP1 / 2 left out - the CPU tfrag fallback's images have the level load's, the
    // chain the frame's VRAM placement; the world's level textures are found by CLAMP's MINV index, the placement is not read)
    // (and TEX0's TBP0 / CBP: the frame's placement too)
    auto vram_masked = [](const Kicked& k, size_t i) {
        std::array<uint32_t, 4> q = k.q[i];
        if (i >= k.part.size() || k.part[i] != 1) return q;
        const uint32_t r = q[2] & 0xff;
        if (r == 0x34 || r == 0x35) q[0] = q[1] = 0;
        if (r == 0x06 || r == 0x07) {
            q[0] &= ~0x3fffu;
            q[1] &= ~(0x3fffu << 5);
        }
        return q;
    };
    auto same_kicked = [&](const Kicked& x, const Kicked& y) {
        if (only_tfrag && x.q.size() == y.q.size() && x.part == y.part) {
            for (size_t i = 0; i < x.q.size(); ++i)
                if (vram_masked(x, i) != vram_masked(y, i)) return false;
        } else if (x.q != y.q) {
            return false;
        }
        if (x.part != y.part || x.clip != y.clip || x.hp.size() != y.hp.size()) return false;
        for (size_t k = 0; k < x.hp.size(); ++k)
            if (std::memcmp(x.hp[k].data(), y.hp[k].data(), 16) != 0) return false;
        return true;
    };
    bool ok = true;
    if (w.packets.size() != d.packets.size()) ok = bad("packets " + std::to_string(w.packets.size()) + " walker, " + std::to_string(d.packets.size()) + " direct");
    // the register writes before each packet (the same packet index in both when the packets match)
    if (w.regs.size() != d.regs.size()) ok = bad("register writes " + std::to_string(w.regs.size()) + " walker, " + std::to_string(d.regs.size()) + " direct");
    for (size_t k = 0; ok && k < w.regs.size(); ++k) {
        const SceneReg &x = w.regs[k], &y = d.regs[k];
        if (x.before != y.before || x.reg != y.reg || x.value != y.value) {
            char t[200];
            std::snprintf(t, sizeof t, "register write %zu: walker before %u reg %02x %016llx, direct before %u reg %02x %016llx", k, x.before, x.reg, static_cast<unsigned long long>(x.value), y.before,
                          y.reg, static_cast<unsigned long long>(y.value));
            ok = bad(t);
        }
    }
    st.regs += w.regs.size();
    for (size_t i = 0; ok && i < w.packets.size(); ++i) {
        const ScenePacket &x = w.packets[i], &y = d.packets[i];
        ++st.packets;
        const std::string at = "packet " + std::to_string(i) + ": ";
        if (x.prog != y.prog || x.start != y.start || x.seq != y.seq || x.table != y.table || x.state != y.state || x.sky != y.sky || x.gif != y.gif || x.shadow != y.shadow ||
            x.shadow_views != y.shadow_views || x.scr_k != y.scr_k || !same_packet(x.tie, y.tie, x.shrub, y.shrub) || x.ntex != y.ntex || (x.tfrag < 0) != (y.tfrag < 0)) {
            char t[240];
            std::snprintf(t, sizeof t, "prog %d/%d start %x/%x seq %u/%u table %d/%d state %d/%d sky %d/%d gif %d/%d tfrag %d/%d tie %d/%d shrub %d/%d", static_cast<int>(x.prog), static_cast<int>(y.prog),
                          x.start, y.start, x.seq, y.seq, x.table, y.table, x.state, y.state, x.sky, y.sky, x.gif, y.gif, x.tfrag, y.tfrag, x.tie != nullptr, y.tie != nullptr, x.shrub != nullptr,
                          y.shrub != nullptr);
            ok = bad(at + t);
            break;
        }
        if (std::memcmp(x.scr_scale, y.scr_scale, sizeof x.scr_scale) != 0 || std::memcmp(x.scr_off, y.scr_off, sizeof x.scr_off) != 0) ok = bad(at + "screen formula");
        if (!same_view(x.view, y.view)) ok = bad(at + "view");
        if (!same_kicked(x.k, y.k)) {
            size_t q = 0;
            while (q < x.k.q.size() && q < y.k.q.size() && x.k.q[q] == y.k.q[q] && (q >= x.k.hp.size() || q >= y.k.hp.size() || std::memcmp(x.k.hp[q].data(), y.k.hp[q].data(), 16) == 0)) ++q;
            char t[300];
            const auto wq = q < x.k.q.size() ? x.k.q[q] : std::array<uint32_t, 4>{}, dq = q < y.k.q.size() ? y.k.q[q] : std::array<uint32_t, 4>{};
            std::snprintf(t, sizeof t, "kicks: qwords %zu walker, %zu direct; first difference %zu (part %d): walker %08x %08x %08x %08x direct %08x %08x %08x %08x", x.k.q.size(), y.k.q.size(), q,
                          q < x.k.part.size() ? x.k.part[q] : -1, wq[0], wq[1], wq[2], wq[3], dq[0], dq[1], dq[2], dq[3]);
            ok = bad(at + t);
        }
        if ((x.sky || x.gif) && !same_writes(x.sky_first, x.sky_count, y.sky_first, y.sky_count)) ok = bad(at + "GIF / sky writes");
        if (x.tie != nullptr || x.shrub != nullptr) {
            if (x.inst >= w.insts.size() || y.inst >= d.insts.size()) {
                ok = bad(at + "instance index");
            } else {
                const SceneInst &a = w.insts[x.inst], &b = d.insts[y.inst];
                if (std::memcmp(a.rows, b.rows, sizeof a.rows) != 0) ok = bad(at + "instance rows");
                else if (std::memcmp(a.morph, b.morph, sizeof a.morph) != 0) ok = bad(at + "instance morph");
                else if (a.fog != b.fog || a.alpha != b.alpha || a.resident != b.resident) ok = bad(at + "instance F / alpha / resident");
                else if (a.resident && a.colour_first != b.colour_first) ok = bad(at + "instance colour slot");
                else if (!a.resident) {
                    const uint32_t nc = x.tie != nullptr ? kTieColours : kShrubColours;
                    if (a.colour_first + nc > w.colours.size() || b.colour_first + nc > d.colours.size() ||
                        !std::equal(w.colours.begin() + a.colour_first, w.colours.begin() + a.colour_first + nc, d.colours.begin() + b.colour_first))
                        ok = bad(at + "instance colours");
                }
            }
            if (x.tex + x.ntex > w.tex.size() || y.tex + y.ntex > d.tex.size()) {
                ok = bad(at + "texture qwords");
            } else if (!std::equal(w.tex.begin() + x.tex, w.tex.begin() + x.tex + x.ntex, d.tex.begin() + y.tex, [chained](const auto& p, const auto& q) {
                           return chained ? tex_placed(p) == tex_placed(q) : p == q;
                       })) {
                uint32_t q = 0;
                while (q + 1 < x.ntex && (chained ? tex_placed(w.tex[x.tex + q]) == tex_placed(d.tex[y.tex + q]) : w.tex[x.tex + q] == d.tex[y.tex + q])) ++q;
                const auto& wq = w.tex[x.tex + q];
                const auto& dq = d.tex[y.tex + q];
                char t[200];
                std::snprintf(t, sizeof t, "texture qword %u of %u: walker %08x %08x %08x %08x, direct %08x %08x %08x %08x", q, x.ntex, wq[0], wq[1], wq[2], wq[3], dq[0], dq[1], dq[2], dq[3]);
                ok = bad(at + t);
            }
        }
        if (x.tfrag >= 0) {
            const TfragDraw &a = w.tfrag.draws[static_cast<size_t>(x.tfrag)], &b = d.tfrag.draws[static_cast<size_t>(y.tfrag)];
            if (a.entry != b.entry || a.lod != b.lod || a.code != b.code || a.b != b.b) ok = bad(at + "tfrag draw");
            else if (w.tfrag.views[a.view] != d.tfrag.views[b.view]) ok = bad(at + "tfrag view");
            else if (w.tfrag.consts[a.consts] != d.tfrag.consts[b.consts]) ok = bad(at + "tfrag constants");
        }
    }
    if (ok && (w.has_sky != d.has_sky || w.sky_tex != d.sky_tex || w.sky_asset != d.sky_asset || w.sky_a != d.sky_a)) ok = bad("the sky's textures / asset / projection");
    if (chained && g_walkers[0] != nullptr && g_walkers[1] != nullptr && g_walkers[1]->twice) {
        // (the memory a kTwiceBase vertex can read: 0x22 quadwords from twice its instance's colours' address - the blocks 0xee /
        // 0x17b + 0x1c i, i < 5 -, wrapped at 0x400)
        const uint8_t *a = g_walkers[0]->vu.st.mem.data(), *b = g_walkers[1]->vu.st.mem.data();
        uint32_t q = 0x1e6;
        while (q < 0x402 && std::memcmp(a + ((q & 0x3ff) << 4), b + ((q & 0x3ff) << 4), 16) == 0) ++q;
        char t[96];
        std::snprintf(t, sizeof t, "VU1 quadword 0x%03x", q & 0x3ff);
        g_cstats.memory.check(q == 0x402, t);
    }
    return st.check.check(ok, why);
}

// orig: (host) see mrp_world_scene.h
namespace {
// orig: (host) scene_build's work; `chained`: the chained world asked for (`chained_bad`: the chain's draw starts were not the
// notes': the scene is to be made again without them)
bool scene_build_run(const WorldIndex& idx, const SceneInput& in, SceneFrame& f, bool reset, bool kicks, bool chained, bool& chained_bad) {
    const uint32_t chain_start = in.chain_start, chain_end = in.chain_end;
    static const bool prof = std::getenv("RAC1_FRAME_PROF") != nullptr;  // (the per-program timers only when profiling)
    using clock = std::chrono::steady_clock;
    f.packets.clear();
    f.insts.clear();
    f.colours.clear();
    {  // (host) the static level's texture table kept at 0 over the frames (SceneFrame::static_tex_base)
        static const bool keep_on = std::getenv("RAC1_PERF_TEXTABLE") == nullptr || std::atoi(std::getenv("RAC1_PERF_TEXTABLE")) != 0;
        const StaticLevel* sl = in.stat_level;
        if (keep_on && sl != nullptr && in.stat != nullptr && !in.stat_check) {
            const uint32_t n = static_cast<uint32_t>(sl->tex.size());
            if (f.static_tex_base == 0 && f.static_tex_gen == sl->gen && f.static_tex_n == n && f.tex.size() >= n) f.tex.resize(n);
            else f.tex.assign(sl->tex.begin(), sl->tex.end());
            f.static_tex_base = 0;
            f.static_tex_gen = sl->gen;
            f.static_tex_n = n;
        } else {
            f.tex.clear();
            f.static_tex_base = -1;
            f.static_tex_gen = f.static_tex_n = 0;
        }
    }
    f.regs.clear();
    f.views.clear();
    f.sky.clear();
    f.sky_tex.clear();
    f.sky_asset = f.sky_packets = f.sky_kicks = f.gif_packets = 0;
    f.has_sky = false;
    f.ms_sky = 0;
    std::fill(std::begin(f.draws), std::end(f.draws), 0);
    std::fill(std::begin(f.unknown), std::end(f.unknown), 0);
    f.setups = f.unknown_program = 0;
    std::fill(std::begin(f.ms_setup), std::end(f.ms_setup), 0.0);
    std::fill(std::begin(f.ms_draw), std::end(f.ms_draw), 0.0);
    f.draw_starts = 0;
    f.resident = f.copied = f.verified = f.verify_bad = 0;
    for (int k = 0; k < 2; ++k) f.stat_draws[k] = f.chk_walker[k] = f.chk_found[k] = f.chk_missing[k] = f.chk_extra[k] = 0;
    f.chk_morph_bad = f.chk_fog_bad = f.chk_alpha_bad = f.chk_rows_bad = 0;
    f.stat_variants = f.chk_gif_frames = f.chk_gif_bad = f.chk_gif_quads = 0;
    f.chk_rows_maxrel = 0;
    f.chk_notes.clear();
    f.tfrag.clear();
    f.tfrag_checked = f.tfrag_bad = 0;
    f.tfrag_host = f.tfrag_cpu = 0;
    f.tchk_walker = f.tchk_found = f.tchk_missing = f.tchk_extra = f.tchk_code = 0;
    f.ok = false;
    if (!in.ok) return false;
    Walker*& g_walker = g_walkers[in.walker != 0 ? 1 : 0];
    if (g_walker == nullptr || reset) {
        delete g_walker;
        g_walker = new Walker();
        g_walker->vu.st.vf[0].f[3] = 1.0f;  // (vf0 = (0, 0, 0, 1), as the hardware's and vu1_global()'s: the sky sprites' particle
                                            // program reads its w - their Z was made without the view's translation row)
    }
    Walker& W = *g_walker;
    vu1::Vu1& vu = W.vu;
    uint32_t last_end = 0;
    for (const PassRange& r : in.passes) last_end = std::max(last_end, r.end);
    auto in_pass = [&in](uint32_t addr, bool sky) {
        for (const PassRange& r : in.passes)
            if ((r.pass == 3) == sky && addr >= r.first && addr < r.end) return true;
        return false;
    };
    // the sky: its streams' register writes as packets of their own (in chain order: before the world's)
    SkyGif sky_gif;
    auto sky_packet = [&f](uint32_t first, bool kick) {
        if (f.sky.size() == first) return;
        ScenePacket p;
        p.sky = true;
        p.table = 3;
        p.seq = 0xffffffffu;  // (not a world draw: the check's grouping ignores it)
        p.sky_first = first;
        p.sky_count = static_cast<uint32_t>(f.sky.size()) - first;
        f.packets.push_back(std::move(p));
        ++f.sky_packets;
        if (kick) ++f.sky_kicks;
    };
    // the particle program's kicks (Veldin's sprites): a PATH1 packet each
    const vu1::KickSink sky_kick = [&f, &sky_packet](const uint8_t* data, uint32_t qwc) {
        const uint32_t first = static_cast<uint32_t>(f.sky.size());
        SkyGif g;
        g.data(data, qwc, f.sky);
        sky_packet(first, true);
    };
    Build b{idx, f, in};
    for (int k = 0; k < 2; ++k) b.discard[k] = in.stat != nullptr && in.stat_level != nullptr && !in.stat_check && in.stat->on[k];
    b.tfrag_discard = in.tfrag_host != nullptr && in.tfrag_level != nullptr && !in.tfrag_check;
    // (the pass the walk is in, the host's ties / shrubs drawn when it leaves the tie / shrub pass's range)
    auto pass_of = [&in](uint32_t addr) {
        for (const PassRange& r : in.passes)
            if (addr >= r.first && addr < r.end) return r.pass;
        return -1;
    };
    auto pass_first = [&in](uint32_t addr) {
        for (const PassRange& r : in.passes)
            if (addr >= r.first && addr < r.end) return r.first;
        return 0u;
    };
    int pass_prev = -1;
    b.kicks = kicks;
    b.walker = &W;
    // (the chained world: not with the checks that take the walk's own draws or kicks)
    b.ch_on = chained && !kicks && !in.stat_check && !in.tfrag_check;
    // the program the micro memory holds now
    auto cur_prog = [&] {
        if (W.micro_dirty) {
            W.prog = program_of(vu.micro.data());
            W.micro_dirty = false;
        }
        return W.prog;
    };
    // (`chained_draw`: the range's draws - 0 tie A's, 1 shrub A's, 2 the tfrags' - are the pass's notes': a draw start is made
    // from them)
    auto on_start = [&](uint32_t start, bool world, int chained_draw = -1) {
        // the double buffer (vu1.cpp start_program)
        vu.top = vu.tops;
        vu.itop = vu.itops;
        vu.dbf = !vu.dbf;
        vu.tops = (vu.base + (vu.dbf ? vu.ofst : 0)) & 0x3ff;
        vu.st.xtop = vu.top;
        if (chained_draw >= 0 && is_draw_start(cur_prog(), start)) {
            if (chained_draw == 0) draw_tie_chained(b, vu.st);
            else if (chained_draw == 1) draw_shrub_chained(b, vu.st);
            else draw_tfrag_chained(b, vu.st, W.prog, start);
            ++f.draw_starts;
            return;
        }
        if (!world) return;
        cur_prog();
        if (W.prog == Program::None) {
            ++f.unknown_program;
            return;
        }
        const auto t0 = prof ? clock::now() : clock::time_point{};
        if (!is_draw_start(W.prog, start)) {
            ++f.setups;
            b.hit_ok = false;
            ++f.setup_starts[static_cast<int>(W.prog) * 0x20 + static_cast<int>(start & 0x1f)];
            run_program(vu.st, W.prog, start);
            if (prof) f.ms_setup[static_cast<int>(W.prog)] += std::chrono::duration<double, std::milli>(clock::now() - t0).count();
            return;
        }
        switch (W.prog) {
        case Program::ShrubA: case Program::ShrubB: draw_shrub(b, vu.st, W.prog); break;
        case Program::TieA: case Program::TieB: draw_tie(b, vu.st, W.prog); break;
        default: draw_tfrag(b, vu.st, W.prog, start); break;
        }
        if (prof) f.ms_draw[static_cast<int>(W.prog)] += std::chrono::duration<double, std::milli>(clock::now() - t0).count();
        ++f.draw_starts;
    };
    std::vector<uint32_t> stack;
    std::vector<uint8_t> buf, span;
    uint32_t qwc_short = 0xffffffffu;  // (a shadow chain's copy shorter than the tag's data: the quadwords it has)
    GifPath gif;
    static const bool no_gif = std::getenv("RAC1_MRPW_GIF") != nullptr && std::atoi(std::getenv("RAC1_MRPW_GIF")) == 0;  // (0 the world ranges' GIF vertices not drawn, as before)
    // A pass's range replaced by its shadow chain (EE addresses [base, base + size) read from the copy)
    const Shadow* sh = nullptr;
    // (the EE reads from the input's copies, in the capture's order)
    size_t span_at = 0;
    auto mem_at = [&sh, &in, &span_at](uint32_t a) -> const uint8_t* {
        if (sh != nullptr && a >= sh->base && a - sh->base < sh->bytes.size()) return sh->bytes.data() + (a - sh->base);
        if (span_at < in.spans.size() && in.spans[span_at][0] == a) {
            const auto& sp = in.spans[span_at++];
            return sp[2] == 0xffffffffu ? nullptr : in.bytes.data() + sp[1];
        }
        ++g_capture_misses;  // (never seen: the walk and the capture follow the same tags)
        return static_cast<const uint8_t*>(ee_ram_ptr_read(a));
    };
    uint32_t addr = chain_start & ee_dma_addr_mask();
    for (int guard = 0; guard < 400000; ++guard) {
        if (sh == nullptr) {
            if (addr == (chain_end & ee_dma_addr_mask()) || addr == last_end) break;
            if (in.dw.ok && addr == in.dw.start) break;  // (the direct world: the world's ranges were scanned at the capture)
            for (const Shadow& s : in.shadows)
                if (s.first == addr) {
                    sh = &s;
                    addr = s.base;
                }
        } else if (addr < sh->base || addr - sh->base >= sh->bytes.size()) {  // (the shadow chain's end: on after the real range)
            addr = sh->end;
            sh = nullptr;
            continue;
        }
        const uint8_t* t = mem_at(addr);
        if (t == nullptr) break;
        const uint64_t lo = r64(t);
        const uint32_t qwc = lo & 0xffff, id = (lo >> 28) & 7;
        const uint32_t a = static_cast<uint32_t>(lo >> 32) & ee_dma_addr_mask();
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
        // the tag's stream: its upper 64 bits (stream offsets 8 and 12) then its data, read in place (no copy; a code's
        // data across the tag's end into its data, or a stream across the shadow chain's end, from a copy)
        uint8_t hdr[16] = {};
        std::memcpy(hdr + 8, t + 8, 8);
        const uint8_t* dp = hdr + 16;
        if (qwc != 0) {
            const uint8_t* d = mem_at(data);
            if (d == nullptr) break;
            if (sh != nullptr && data >= sh->base && data - sh->base < sh->bytes.size() && data - sh->base + qwc * 16 > sh->bytes.size()) {  // (across the shadow chain's end)
                buf.clear();
                for (uint32_t q = 0; q < qwc; ++q) {
                    const uint8_t* dq = mem_at(data + q * 16);
                    if (dq == nullptr) break;
                    buf.insert(buf.end(), dq, dq + 16);
                }
                dp = buf.data();
                if (buf.size() < qwc * 16u) qwc_short = static_cast<uint32_t>(buf.size() / 16);
            } else {
                dp = d;
            }
        }
        const uint32_t n = 16 + (qwc_short != 0xffffffffu ? qwc_short : qwc) * 16;
        qwc_short = 0xffffffffu;
        auto at = [&hdr, &dp, &span](uint32_t i, uint32_t len) -> const uint8_t* {
            if (i >= 16) return dp + (i - 16);
            if (i + len <= 16) return hdr + i;
            span.assign(hdr + i, hdr + 16);
            span.insert(span.end(), dp, dp + (i + len - 16));
            return span.data();
        };
        const bool sky = sh != nullptr ? sh->pass == 3 : in_pass(addr, true);
        const int pass_now = sh != nullptr ? sh->pass : pass_of(addr);
        if (pass_prev != pass_now && (pass_prev == 1 || pass_prev == 2)) emit_static(b, vu.st, pass_prev - 1);
        if (pass_prev != pass_now) {  // (the chained world: the range left, the range entered)
            if (pass_prev == 1 || pass_prev == 2) chained_leave(b, vu.st, pass_prev - 1);
            if (pass_prev == 0) chained_tfrag_leave(b);
            if (sh == nullptr && (pass_now == 1 || pass_now == 2)) chained_enter(b, pass_now - 1, pass_first(addr));
            if (sh == nullptr && pass_now == 0) chained_tfrag_enter(b, pass_first(addr));
        }
        if (pass_prev != pass_now && pass_prev == 0) {
            emit_tfrag_host(b, vu.st);
            emit_tfrag_shadow(b, vu.st);
        }
        pass_prev = pass_now;
        const bool world = sh != nullptr ? sh->pass != 3 : (!sky && in_pass(addr, false));
        // (a pass whose instances the host makes: only its constants are unpacked and their start run - the tie A alpha-test state
        // packet's qwords 0x3cc..0x3ce (constants at 0x3c6, start 0x008), the shrub fog constants (qwords 0..5, start 0x000) the host's
        // instances take; its packets' images, set-up starts, instance data and draws are not processed. RAC1_MRPW_STATIC_WALK=1
        // (testing): all of it walked as for the real draws (their draws not decoded))
        static const bool walk_all = std::getenv("RAC1_MRPW_STATIC_WALK") != nullptr;
        const bool skip = !walk_all && sh == nullptr && (pass_now == 1 || pass_now == 2) && b.discard[pass_now - 1];
        // (the chained world: this range's tie A / shrub A part is the pass's notes' - its unpacks and set-up starts left out as a
        // skipped pass's, its draw starts made from the notes - while the micro memory holds that program; the pass's other
        // programs' parts are walked)
        const int ck = sh == nullptr && (pass_now == 1 || pass_now == 2) && b.chained[pass_now - 1] ? pass_now - 1 : -1;
        auto chained_part = [&] { return ck >= 0 && cur_prog() == (ck == 0 ? Program::TieA : Program::ShrubA); };
        for (uint32_t i = 0; i + 4 <= n;) {
            const uint32_t code = r32(at(i, 4));
            i += 4;
            const uint32_t cmd = (code >> 24) & 0x7f, imm = code & 0xffff;
            if (cmd == 0x50 || cmd == 0x51) {  // DIRECT / DIRECTHL: the GS registers it writes
                const uint32_t q = imm == 0 ? 65536 : imm;
                i = (i + 15) & ~15u;
                const uint32_t take = std::min(q * 16, n - std::min(n, i));
                if (sky) {  // (the sky's: drawn, its writes a packet of their own)
                    const auto t0 = prof ? clock::now() : clock::time_point{};
                    const uint32_t first = static_cast<uint32_t>(f.sky.size());
                    sky_gif.data(at(i, take), take / 16, f.sky);
                    sky_packet(first, false);
                    if (prof) f.ms_sky += std::chrono::duration<double, std::milli>(clock::now() - t0).count();
                } else {
                    if (b.in.stat_check && pass_now == 2)
                        for (uint32_t k = 0; k + 16 <= take; k += 16) {
                            std::array<uint32_t, 4> w;
                            std::memcpy(w.data(), at(i + k, 16), 16);
                            b.chk_gif.push_back(w);
                        }
                    gif.draw = world && (pass_now < 1 || pass_now > 2 || !b.discard[pass_now - 1]) && !no_gif;
                    gif.build = &b;
                    gif.table = pass_now;
                    gif.data(at(i, take), take / 16, f);
                }
                i += q * 16;
                continue;
            }
            const uint32_t nb = vu1::vif_data_bytes(vu, code);
            if (nb != 0 && i + nb > n) break;
            if (sky) {  // the sky's VIF codes (its sprites: the particle program's MPG, its data, its starts) on the private VU1, run as the real VU1 does
                const auto t0 = prof ? clock::now() : clock::time_point{};
                if (cmd == 0x4a) W.micro_dirty = true;
                vu1::vif1_command(vu, code, at(i, nb), sky_kick);
                if (prof) f.ms_sky += std::chrono::duration<double, std::milli>(clock::now() - t0).count();
                i += nb;
                continue;
            }
            if (cmd == 0x4a) b.hit_ok = false;
            // (the constants' starts and UNPACK destinations of a skipped pass)
            auto constants_start = [&](uint32_t start) { return pass_now == 1 ? (start & 0x3ff) == 0x008 : (start & 0x3ff) == 0x000; };
            auto constants_dest = [&]() {
                const uint32_t d = ((imm & 0x3ff) + ((imm & 0x8000) != 0 ? vu.tops : 0)) & 0x3ff;
                return pass_now == 1 ? d >= 0x3c6 && d < 0x3d0 : d < 6;
            };
            // (the chained world: the tie A part's colour tables before the pass's next part is walked)
            if (ck == 0 && b.ch_tables && world && (cmd == 0x14 || cmd == 0x15 || cmd == 0x17 || cmd >= 0x60) && !chained_part()) chained_tables(b, vu.st);
            // (a level whose shrubs read VU1 memory the tie pass leaves, Build::ch_tables: of the tie A part only the instances'
            // unpacks are left out - their matrix at the double buffer's block, their colours at 0x346 / 0x386, which
            // chained_tables puts back - and its packets' unpacks and set-up starts are walked: that memory as the walk leaves it)
            const bool keep = ck == 0 && b.ch_tables;
            auto instance_dest = [&]() { return (imm & 0x8000) != 0 || (imm & 0x3ff) >= 0x346; };
            // (the chained world's tfrag range: its draw starts are the notes', its other starts - the constants' - are run; of its
            // unpacks the entries' - at the double buffer's base - are left out, the pass's own - the view, the constants - kept)
            const bool ctf = sh == nullptr && pass_now == 0 && b.ch_tf && world;
            if (ctf && (cmd == 0x14 || cmd == 0x15 || cmd == 0x17)) {
                on_start(cmd == 0x17 ? vu.next_pc : imm, true, 2);
                if (cmd != 0x17) vu.next_pc = imm;
            } else if (cmd == 0x14 || cmd == 0x15) {
                const bool cp = world && chained_part();
                on_start(imm, world && (!(skip || (cp && !keep)) || constants_start(imm)), cp ? ck : -1);
                vu.next_pc = imm;
            } else if (cmd == 0x17) {
                const bool cp = world && chained_part();
                on_start(vu.next_pc, world && (!(skip || (cp && !keep)) || constants_start(vu.next_pc)), cp ? ck : -1);
            } else if (cmd == 0x4a || cmd >= 0x60) {  // MPG / UNPACK: the world's only
                const bool run = cmd == 0x4a ? true
                                 : ctf ? (imm & 0x8000) == 0 || b.ch_tf_keep
                                       : !(skip || (chained_part() && (!keep || instance_dest()))) || constants_dest();
                if (world && run) {
                    if (cmd == 0x4a) W.micro_dirty = true;
                    else W.unpack_src[((imm & 0x3ff) + ((imm & 0x8000) != 0 ? vu.tops : 0)) & 0x3ff] = i >= 16 ? data + (i - 16) : 0;
                    vu1::vif1_command(vu, code, at(i, nb), nullptr);
                }
            } else {
                vu1::vif1_command(vu, code, at(i, nb), nullptr);
            }
            i += nb;
        }
        if (stop) {
            if (sh == nullptr) break;
            next = sh->end;  // (a shadow chain's end tag: on after the real range)
            sh = nullptr;
        }
        addr = next;
    }
    if (in.dw.ok) {  // the direct world: the scan's register writes, GIF vertices and the host's draws, in the walk's order
        static vu1::Vu1* tmp = new vu1::Vu1();  // (the VU1 state each pass's chain left: what emit_* read)
        for (const DirectEvent& e : in.dw.events) {
            if (e.kind == 0) {
                f.regs.push_back({static_cast<uint32_t>(f.packets.size()), e.reg, e.value});
                continue;
            }
            if (e.kind == 1) {
                ScenePacket p;
                p.gif = true;
                p.table = e.table;
                p.view = b.view(false);
                p.seq = 0xffffffffu;
                p.sky_first = static_cast<uint32_t>(f.sky.size());
                p.sky_count = e.count;
                f.sky.insert(f.sky.end(), in.dw.writes.begin() + e.first, in.dw.writes.begin() + e.first + e.count);
                f.packets.push_back(std::move(p));
                ++f.gif_packets;
                continue;
            }
            direct_state_put(in.dw.states[e.state], tmp->st);
            f.draw_starts = e.draw_starts;
            if (e.kind == 2) {
                emit_tfrag_host(b, tmp->st);
                emit_tfrag_shadow(b, tmp->st);
                emit_tfrag_cpu(b, tmp->st);  // (the CPU tfrag fallback's)
            } else {
                emit_static(b, tmp->st, e.kind - 3);
            }
        }
        f.draw_starts = in.dw.draw_starts;
        ++g_dstats.frames;
    } else {
        emit_tfrag_host(b, vu.st);  // (the walk ended in the tfrag pass)
        emit_tfrag_shadow(b, vu.st);
        emit_static(b, vu.st, 0);  // (a pass the walk ended in)
        emit_static(b, vu.st, 1);
    }
    if (pass_prev == 1 || pass_prev == 2) chained_leave(b, vu.st, pass_prev - 1);  // (the chained world: a range the walk ended in)
    if (pass_prev == 0) chained_tfrag_leave(b);
    chained_bad = b.ch_bad;
    if (b.ch_used && !b.ch_bad) ++g_cstats.scenes;
    emit_host_meshes(b);  // (the level mods' host meshes, after the world)
    static_check(b);
    // the sky (captured with the input)
    f.has_sky = in.has_sky;
    if (f.has_sky) {
        f.sky_asset = in.sky_asset;
        f.sky_tex = in.sky_tex;
        for (size_t k = 0; k < 16; ++k) f.sky_a[k] = in.sky_a[k];
    }
    f.ok = true;
    return true;
}
}  // namespace

// orig: (host) see mrp_world_scene.h
bool scene_build(const WorldIndex& idx, const SceneInput& in, SceneFrame& f, bool reset, bool kicks) {
    bool bad = false;
    bool ok = scene_build_run(idx, in, f, reset, kicks, in.chained.want, bad);
    if (bad) {  // (the chained world: the chain's draw starts were not the passes' notes - the walk, as before)
        ++g_cstats.again;
        ok = scene_build_run(idx, in, f, false, kicks, false, bad);
    }
    return ok;
}

// ---- the in-game check -------------------------------------------------------------------------

namespace {
struct CheckDraw {
    Program prog = Program::None;
    uint32_t start = 0;
    std::vector<std::vector<std::array<uint32_t, 4>>> kicks;
};
std::vector<CheckDraw> g_check;
}  // namespace

// orig: (host)
void scene_check_start(Program prog, uint32_t start) {
    if (!is_draw_start(prog, start)) return;
    g_check.push_back(CheckDraw{prog, start, {}});
}

// orig: (host)
void scene_check_kick(const uint8_t* data, uint32_t qwc) {
    if (g_check.empty()) return;
    std::vector<std::array<uint32_t, 4>> pk(qwc);
    std::memcpy(pk.data(), data, static_cast<size_t>(qwc) * 16);
    g_check.back().kicks.push_back(std::move(pk));
}

// orig: (host)
void scene_check_frame(const SceneFrame& f, CheckStats& stats) {
    // the scene's packets grouped by their draw start, against the real starts in order
    std::vector<std::vector<Kicked>> mine(f.draw_starts);
    std::vector<Program> mprog(f.draw_starts, Program::None);
    std::vector<uint32_t> mstart(f.draw_starts, 0);
    for (const ScenePacket& p : f.packets)
        if (p.seq < f.draw_starts) {
            mine[p.seq].push_back(p.k);
            mprog[p.seq] = p.prog;
            mstart[p.seq] = p.start;
        }
    if (g_check.size() != f.draw_starts && stats.notes.size() < 40)
        stats.notes.push_back("frame: " + std::to_string(g_check.size()) + " real draw starts, the scene " + std::to_string(f.draw_starts));
    ++stats.cover["frames"];
    if (g_check.size() != f.draw_starts) ++stats.cover["frames with another draw count"];
    const size_t n = std::min<size_t>(g_check.size(), f.draw_starts);
    for (size_t i = 0; i < n; ++i) {
        const CheckDraw& d = g_check[i];
        if (mine[i].empty() && mprog[i] == Program::None) {  // (a start the scene could not decode: an unknown packet)
            ++stats.cover["draws the scene did not decode"];
            continue;
        }
        if (d.prog != mprog[i] || d.start != mstart[i]) {
            ++stats.cover["draws out of step"];
            continue;
        }
        char what[160];
        std::snprintf(what, sizeof what, "draw %zu start %03x", i, d.start);
        if (d.kicks.size() != mine[i].size() && !d.kicks.empty() && d.kicks[0].size() >= 2) {  // (the first kicked packet, e.g. tie A's state)
            const auto& q = d.kicks[0];
            std::snprintf(what, sizeof what, "draw %zu start %03x prog %d (previous draw prog %d), first kick %08x %08x | %08x %08x %08x", i, d.start, static_cast<int>(d.prog),
                          i > 0 ? static_cast<int>(g_check[i - 1].prog) : -1, q[0][0], q[0][1], q[1][0], q[1][1], q[1][2]);
        }
        compare_draw(d.prog, mine[i], d.kicks, stats, what);
    }
    g_check.clear();
}

}  // namespace rac1::mrpw
