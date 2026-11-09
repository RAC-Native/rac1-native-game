// See host_meshes.h. (The reader: the experiment's custom ties container, generalised.)
#include <formats/host_meshes.h>

#include <algorithm>

#include <cstring>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <iterator>

namespace rac1::mrpw {

namespace {

struct Reader {
    std::vector<uint8_t> b;
    size_t at = 0;
    bool ok = true;
    template <typename T>
    T get() {
        T v{};
        if (!ok || at + sizeof(T) > b.size()) {
            ok = false;
            return v;
        }
        std::memcpy(&v, b.data() + at, sizeof(T));
        at += sizeof(T);
        return v;
    }
    std::vector<uint8_t> bytes(size_t n) {
        if (!ok || at + n > b.size()) {
            ok = false;
            return {};
        }
        std::vector<uint8_t> v(b.begin() + static_cast<long>(at), b.begin() + static_cast<long>(at + n));
        at += n;
        return v;
    }
};

constexpr uint32_t kMagic = 0x4350524du;  // "MRPC"
// the A+D values of RAC1's tie texture prims (Veldin's, as its level load leaves them): TEX1 (MXL 3, bilinear, trilinear
// between the mips, Veldin's K) and TEX0 (T8 / CT32, RGBA, modulate); CLAMP's MINV holds the level texture (the MRP's key)
constexpr uint64_t kTex1 = 0x0000ff920000012cull, kTex0 = 0x8005b5a599304000ull;
// the strips' GIF tag as RAC1's tie pass has it (world_trace.cpp tie_consts): PRE, PRIM 0x7c, NREG 3, STQ RGBAQ XYZF2
constexpr uint64_t kStripTag = 0x303e400000000000ull, kStripRegs = 0x412;
constexpr uint64_t kAdTag = 0x1000000000000005ull, kAdRegs = 0xe;  // (NLOOP 5, NREG 1, A+D)

}  // namespace

// orig: (host) see host_meshes.h
bool read_host_meshes(const std::string& path, HostMeshes& out, std::string& error) {
    out = HostMeshes{};
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = path + " cannot be read";
        return false;
    }
    Reader r;
    r.b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    const uint32_t magic = r.get<uint32_t>(), version = r.get<uint32_t>();
    if (magic != kMagic || version < 1 || version > 2) {
        error = path + ": not a host meshes file (version 1 or 2)";
        return false;
    }
    const uint32_t ntex = r.get<uint32_t>();
    for (uint32_t i = 0; i < ntex && r.ok; ++i) {
        mrp::Texture t;
        t.index = i;
        t.width = r.get<uint32_t>();
        t.height = r.get<uint32_t>();
        t.palette = r.get<std::array<uint32_t, 256>>();
        const uint32_t mips = r.get<uint32_t>();
        for (uint32_t m = 0; m < mips && r.ok; ++m) t.mips.push_back(r.bytes(r.get<uint32_t>()));
        out.textures.push_back(std::move(t));
    }
    const uint32_t ncls = r.get<uint32_t>();
    for (uint32_t ci = 0; ci < ncls && r.ok; ++ci) {
        TieClass c;
        c.o_class = r.get<int32_t>();
        c.index = ci;
        c.scale = r.get<float>();
        float sphere[4];
        for (float& s : sphere) s = r.get<float>();
        // (a RAC1 class header's fields where the host reads them: the sphere +0x30, the scale +0x40)
        c.header.assign(0x70, 0);
        std::memcpy(c.header.data() + 0x30, sphere, sizeof sphere);
        std::memcpy(c.header.data() + 0x40, &c.scale, 4);
        const uint32_t nlt = r.get<uint32_t>();
        c.texture_remap.fill(0xff);
        if (nlt > c.texture_remap.size()) {
            error = path + ": a class with more than 16 textures";
            return false;
        }
        for (uint32_t k = 0; k < nlt && r.ok; ++k) {
            const uint32_t tex = r.get<uint32_t>(), wrap = r.get<uint32_t>();
            const uint64_t alpha = r.get<uint64_t>();
            TexturePrim tp;
            tp.n = 5;
            tp.local_texture = static_cast<int32_t>(k);
            tp.texture = static_cast<int32_t>(tex);
            tp.ad[0] = {kTex1, 0x14};
            tp.ad[1] = {(static_cast<uint64_t>(tex) << 24) | (wrap & 0xf), 0x08};
            tp.ad[2] = {kTex0, 0x06};
            tp.ad[3] = {0, 0x34};
            tp.ad[4] = {alpha, 0x42};
            c.textures.push_back(tp);
            // (the world's load takes a class's textures from its remap, mrp_world_assets.cpp: at most 16)
            if (k < c.texture_remap.size()) c.texture_remap[k] = static_cast<uint8_t>(tex);
        }
        const uint32_t npk = r.get<uint32_t>();
        for (uint32_t pi = 0; pi < npk && r.ok; ++pi) {
            TiePacket p;
            p.lod = r.get<uint8_t>();
            p.index_in_lod = r.get<uint8_t>();
            (void)r.get<uint16_t>();
            p.n_dinky = r.get<uint32_t>();
            p.n_fat = r.get<uint32_t>();
            for (uint32_t k = 0; k < p.n_dinky && r.ok; ++k) p.dinky.push_back(r.get<std::array<int16_t, 8>>());
            for (uint32_t k = 0; k < p.n_fat && r.ok; ++k) p.fat.push_back(r.get<std::array<int16_t, 12>>());
            const uint32_t nstrips = r.get<uint32_t>();
            int slot = -1, slot_tex = -1;
            for (uint32_t s = 0; s < nstrips && r.ok; ++s) {
                const uint32_t lt = r.get<uint32_t>(), n = r.get<uint32_t>();
                if (static_cast<int>(lt) != slot_tex) {  // (a texture change: its slot's A+D qwords)
                    p.prims.push_back(static_cast<int32_t>(lt));
                    slot = static_cast<int>(p.prims.size()) - 1;
                    slot_tex = static_cast<int>(lt);
                    Op a;
                    a.kind = OpKind::Tag;
                    a.tag[0] = kAdTag;
                    a.tag[1] = kAdRegs;
                    p.ops.push_back(a);
                    Op t;
                    t.kind = OpKind::Texture;
                    t.index = static_cast<uint16_t>(slot);
                    p.ops.push_back(t);
                }
                Op tag;
                tag.kind = OpKind::Tag;
                tag.tag[0] = kStripTag | n | (s + 1 == nstrips ? 0x8000ull : 0);
                tag.tag[1] = kStripRegs;
                p.ops.push_back(tag);
                for (uint32_t i = 0; i < n && r.ok; ++i) {
                    const uint16_t v = r.get<uint16_t>();
                    if (v >= p.n_dinky + p.n_fat) {
                        error = path + ": a strip's vertex past the packet's";
                        return false;
                    }
                    TieKickVertex kv;
                    kv.pos = kv.q = kv.uv = v;
                    kv.fat = v >= p.n_dinky ? 1 : 0;
                    if (v < p.n_dinky) kv.colour = static_cast<uint16_t>(p.dinky[v][3] & 63);  // (its palette entry)
                    Op o;
                    o.kind = OpKind::Vertex;
                    o.index = static_cast<uint16_t>(p.verts.size());
                    p.verts.push_back(kv);
                    p.ops.push_back(o);
                }
                if ((n & 1) != 0) (void)r.get<uint16_t>();
            }
            (void)slot;
            p.kick_ok = true;
            p.kick_note = "level mod (host_meshes.mrpc)";
            c.packets.push_back(std::move(p));
        }
        out.classes.push_back(std::move(c));
    }
    const uint32_t ninst = r.get<uint32_t>();
    for (uint32_t i = 0; i < ninst && r.ok; ++i) {
        HostMeshInstance in;
        in.cls = r.get<uint32_t>();
        for (auto& row : in.rot)
            for (float& v : row) v = r.get<float>();
        for (float& v : in.pos) v = r.get<float>();
        for (uint8_t& v : in.rgba) v = r.get<uint8_t>();
        if (version >= 2) {
            const uint32_t n = r.get<uint32_t>();
            if (n > 64) {
                error = path + ": a palette of more than 64 colours";
                return false;
            }
            for (uint32_t k = 0; k < n && r.ok; ++k) in.palette.push_back(r.get<std::array<uint8_t, 4>>());
        }
        if (in.cls >= out.classes.size()) {
            error = path + ": an instance of no class";
            return false;
        }
        out.instances.push_back(in);
    }
    if (!r.ok) {
        error = path + ": truncated";
        return false;
    }
    return true;
}

namespace {
struct Out {
    std::vector<uint8_t> b;
    template <typename T>
    void put(const T& v) {
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        b.insert(b.end(), p, p + sizeof(T));
    }
};
}  // namespace

// orig: (host) see host_meshes.h
bool write_host_meshes(const std::string& path, const HostMeshFile& f, std::string& error) {
    Out o;
    o.put(kMagic);
    o.put(uint32_t{2});
    o.put(static_cast<uint32_t>(f.textures.size()));
    for (const mrp::Texture& t : f.textures) {
        o.put(t.width);
        o.put(t.height);
        o.put(t.palette);
        o.put(static_cast<uint32_t>(t.mips.size()));
        for (const auto& m : t.mips) {
            o.put(static_cast<uint32_t>(m.size()));
            o.b.insert(o.b.end(), m.begin(), m.end());
        }
    }
    o.put(static_cast<uint32_t>(f.classes.size()));
    for (const HostMeshClass& c : f.classes) {
        o.put(c.id);
        o.put(c.scale);
        for (float s : c.sphere) o.put(s);
        o.put(static_cast<uint32_t>(c.textures.size()));
        for (const HostMeshTexRef& t : c.textures) {
            o.put(t.texture);
            o.put(t.wrap);
            o.put(t.alpha);
        }
        o.put(static_cast<uint32_t>(c.packets.size()));
        for (const HostMeshPacket& p : c.packets) {
            o.put(p.lod);
            o.put(p.index_in_lod);
            o.put(uint16_t{0});
            o.put(static_cast<uint32_t>(p.dinky.size()));
            o.put(static_cast<uint32_t>(p.fat.size()));
            for (const auto& v : p.dinky) o.put(v);
            for (const auto& v : p.fat) o.put(v);
            o.put(static_cast<uint32_t>(p.strips.size()));
            for (const HostMeshStrip& s : p.strips) {
                o.put(s.local_texture);
                o.put(static_cast<uint32_t>(s.verts.size()));
                for (uint16_t v : s.verts) o.put(v);
                if ((s.verts.size() & 1) != 0) o.put(uint16_t{0});
            }
        }
    }
    o.put(static_cast<uint32_t>(f.instances.size()));
    for (const HostMeshInstance& in : f.instances) {
        o.put(in.cls);
        for (const auto& row : in.rot)
            for (float v : row) o.put(v);
        for (float v : in.pos) o.put(v);
        for (uint8_t v : in.rgba) o.put(v);
        o.put(static_cast<uint32_t>(std::min<size_t>(in.palette.size(), 64)));
        for (size_t k = 0; k < in.palette.size() && k < 64; ++k) o.put(in.palette[k]);
    }
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (fp == nullptr || std::fwrite(o.b.data(), 1, o.b.size(), fp) != o.b.size()) {
        if (fp != nullptr) std::fclose(fp);
        error = path + " cannot be written";
        return false;
    }
    std::fclose(fp);
    return true;
}

namespace {
std::mutex g_bind_lock;
const void* g_bound_index = nullptr;
const World* g_bound_world = nullptr;
float g_fog[4] = {};
}  // namespace

// orig: (host) see host_meshes.h
void host_meshes_bind(const void* index, const World* world) {
    std::scoped_lock lock(g_bind_lock);
    g_bound_index = world != nullptr ? index : nullptr;
    g_bound_world = world;
}

// orig: (host) see host_meshes.h
const World* host_meshes_bound(const void* index) {
    std::scoped_lock lock(g_bind_lock);
    return g_bound_index != nullptr && g_bound_index == index ? g_bound_world : nullptr;
}

// orig: (host) see host_meshes.h
bool host_meshes_bound_any() {
    std::scoped_lock lock(g_bind_lock);
    return g_bound_world != nullptr;
}

// orig: (host) see host_meshes.h
void host_meshes_fog_set(const float fog[4]) {
    std::scoped_lock lock(g_bind_lock);
    std::memcpy(g_fog, fog, sizeof g_fog);
}

// orig: (host) see host_meshes.h
void host_meshes_fog(float fog[4]) {
    std::scoped_lock lock(g_bind_lock);
    std::memcpy(fog, g_fog, sizeof g_fog);
}

namespace {
std::mutex g_dyn_m;
std::vector<HostMeshInstance> g_dyn;
}  // namespace

// orig: (host) see host_meshes.h
void host_meshes_dynamic_set(std::vector<HostMeshInstance> v) {
    std::scoped_lock lock(g_dyn_m);
    g_dyn = std::move(v);
}

// orig: (host) see host_meshes.h
void host_meshes_dynamic(std::vector<HostMeshInstance>& out) {
    std::scoped_lock lock(g_dyn_m);
    out = g_dyn;
}

}  // namespace rac1::mrpw
