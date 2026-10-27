// The MRP world's scene: see mrp_world_scene.h. Host code.
#include <game/render/mrp_world_scene.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/world_vu1.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/vu/vu1.h>
#include <runtime/vu/vu_interpreter.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace rac1::mrpw {

namespace {

std::vector<PassRange> g_passes;
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
// a doubleword of EE / VU memory (host-order 32-bit words): its low word first on any host (gs_emu.cpp's r64)
uint64_t r64(const uint8_t* p) {
    uint32_t w[2];
    std::memcpy(w, p, 8);
    return (static_cast<uint64_t>(w[1]) << 32) | w[0];
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
};
Walker* g_walker = nullptr;

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

// orig: (host) tie A / B's draw (micro 0x000), then what the program leaves for the next start: the block
// queue vf22 rotated (MR32) and vi3 from it (0x3cb / 0x3cf, B 0x648 / 0x64c); tie A's alpha-test state (0x3bc..0x3c8)
void draw_tie_impl(Build& b, VUState& st, Program prog) {
    uint8_t* mem = st.mem.data();
    const uint32_t block = static_cast<uint32_t>(st.vi[3]) & 0x3ff;
    const bool clip = prog == Program::TieB;
    // tie A: the instance's state (+5 x) stored at 0x3cb x (0x098 / 0x09d)
    const uint32_t state = w32(mem, block + 5, 0) & 0xffff;
    if (!clip) put32(mem, 0x3cb, 0, state);
    auto finish = [&](bool drawn) {
        if (!clip) {
            const uint32_t prev = w32(mem, 0x3cb, 1) & 0xffff;
            if (state != prev) {  // the A+D packet at 0x3d0: the tag 0x3cc, TEST 0x3ce (a state) or 0x3cd (none), kicked before the draw's
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
    };
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
    const uint32_t tex_base = static_cast<uint32_t>(b.f.tex.size());
    b.f.tex.insert(b.f.tex.end(), sl->tex.begin(), sl->tex.end());
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

// orig: (host) tfrag A / B's LOD starts: the entry the memory holds at XTOP, its strips by the reference
void draw_tfrag(Build& b, VUState& st, Program prog, uint32_t start) {
    const uint8_t* mem = st.mem.data();
    const int lod = tfrag_lod(prog, start);
    const uint32_t base = st.xtop & 0x3ff;
    WorldIndex::Hit hit;
    if (!b.idx.find_tfrag(mem, base, lod, hit)) {
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
    case 0xe: out.push_back(SkyWrite{r64(q), 0, q[ee_byte_addr(8)], 0}); break;
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
    void close(SceneFrame& f) {
        if (open == 0xffffffffu) return;
        if (f.sky.size() > open) {
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
                if (d == 0xe) reg_write(f, q[ee_byte_addr(8)], r64(q));
                else if (d == 0x0) reg_write(f, 0x00, r64(q) & 0x7ff);
                else if (draw && d != 0xf) {
                    if (open == 0xffffffffu) open = static_cast<uint32_t>(f.sky.size());
                    packed_write(d, q, f.sky);
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
    if (!in.ok) return;
    // the projections A (0x166d00, the clipping programs' 0x166d40) and the camera (0x166dc0) the views are made from
    for (uint32_t k = 0; k < 16; ++k) {
        in.a[k] = lwc1(ad.a + 4 * k);
        in.a_clip[k] = lwc1(ad.a_clip + 4 * k);
    }
    for (uint32_t k = 0; k < 3; ++k) in.camera[k] = lwc1(ad.camera + 4 * k);
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
    // the walk as scene_build's (the tags followed, the shadow chains' ranges read from their copies)
    std::vector<uint32_t> stack;
    const Shadow* sh = nullptr;
    auto in_sh = [&sh](uint32_t a) { return sh != nullptr && a >= sh->base && a - sh->base < sh->bytes.size(); };
    uint32_t addr = chain_start & 0x01fffff0u;
    for (int guard = 0; guard < 400000; ++guard) {
        if (sh == nullptr) {
            if (addr == (chain_end & 0x01fffff0u) || addr == last_end) break;
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
            lo = r64(sh->bytes.data() + (addr - sh->base));
        } else {
            const uint8_t* t = take(addr, 16);
            if (t == nullptr) break;
            lo = r64(t);
        }
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

// orig: (host) see mrp_world_scene.h: the capture and the build at once (the testing modes)
bool scene_build(const WorldIndex& idx, uint32_t chain_start, uint32_t chain_end, SceneFrame& f, bool reset, bool kicks) {
    static SceneInput in;
    scene_capture(chain_start, chain_end, in);
    return scene_build(idx, in, f, reset, kicks);
}

// orig: (host) see mrp_world_scene.h
bool scene_build(const WorldIndex& idx, const SceneInput& in, SceneFrame& f, bool reset, bool kicks) {
    const uint32_t chain_start = in.chain_start, chain_end = in.chain_end;
    static const bool prof = std::getenv("RAC1_FRAME_PROF") != nullptr;  // (the per-program timers only when profiling)
    using clock = std::chrono::steady_clock;
    f.packets.clear();
    f.insts.clear();
    f.colours.clear();
    f.tex.clear();
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
    int pass_prev = -1;
    b.kicks = kicks;
    b.walker = &W;
    auto on_start = [&](uint32_t start, bool world) {
        // the double buffer (vu1.cpp start_program)
        vu.top = vu.tops;
        vu.itop = vu.itops;
        vu.dbf = !vu.dbf;
        vu.tops = (vu.base + (vu.dbf ? vu.ofst : 0)) & 0x3ff;
        vu.st.xtop = vu.top;
        if (!world) return;
        if (W.micro_dirty) {
            W.prog = program_of(vu.micro.data());
            W.micro_dirty = false;
        }
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
    uint32_t addr = chain_start & 0x01fffff0u;
    for (int guard = 0; guard < 400000; ++guard) {
        if (sh == nullptr) {
            if (addr == (chain_end & 0x01fffff0u) || addr == last_end) break;
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
            if (cmd == 0x14 || cmd == 0x15) {
                on_start(imm, world && (!skip || constants_start(imm)));
                vu.next_pc = imm;
            } else if (cmd == 0x17) {
                on_start(vu.next_pc, world && (!skip || constants_start(vu.next_pc)));
            } else if (cmd == 0x4a || cmd >= 0x60) {  // MPG / UNPACK: the world's only
                if (world && (!skip || cmd == 0x4a || constants_dest())) {
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
    emit_tfrag_host(b, vu.st);  // (the walk ended in the tfrag pass)
    emit_tfrag_shadow(b, vu.st);
    emit_static(b, vu.st, 0);  // (a pass the walk ended in)
    emit_static(b, vu.st, 1);
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
