// See custom_ties.h.
#include <formats/custom_ties.h>

#include <cstring>
#include <fstream>
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

// orig: (host) see custom_ties.h
bool read_custom_ties(const std::string& path, CustomTies& out, std::string& error) {
    out = CustomTies{};
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = path + " cannot be read";
        return false;
    }
    Reader r;
    r.b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    if (r.get<uint32_t>() != kMagic || r.get<uint32_t>() != 1) {
        error = path + ": not a custom ties file (version 1)";
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
            p.kick_note = "custom level (ties.mrpc)";
            c.packets.push_back(std::move(p));
        }
        out.classes.push_back(std::move(c));
    }
    const uint32_t ninst = r.get<uint32_t>();
    for (uint32_t i = 0; i < ninst && r.ok; ++i) {
        CustomTieInstance in;
        in.cls = r.get<uint32_t>();
        for (auto& row : in.rot)
            for (float& v : row) v = r.get<float>();
        for (float& v : in.pos) v = r.get<float>();
        for (uint8_t& v : in.rgba) v = r.get<uint8_t>();
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

}  // namespace rac1::mrpw
