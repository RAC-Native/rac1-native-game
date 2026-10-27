// The MRP moby asset: decoding a class's packets, the file format, the CPU reference of the
// skinning (see moby_mrp.h).
#include <formats/moby_mrp.h>

#include "io/host_endian.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

namespace rac1::mrp {

namespace {

// (the class's bytes are the PS2's: little-endian)
template <typename T> T rd(std::span<const uint8_t> b, size_t at) {
    T v{};
    if (at + sizeof(T) <= b.size()) v = le_load<T>(b.data() + at);
    return v;
}

// A pointer the class stores: an offset into the class (the file) or an EE address (the copy in
// memory: minus the class's address).
uint32_t off(uint32_t p, uint32_t base) { return p >= base ? p - base : p; }

struct Unpack {
    uint32_t vn = 0, vl = 0, num = 0, addr = 0;
    bool tops = false, usn = false, masked = false;
    size_t data = 0;  // offset of its data in the list
    size_t bytes = 0;
};

// The VIF list's codes: only UNPACKs (and NOP padding) are expected in a moby packet's list.
bool parse_vif(std::span<const uint8_t> v, std::vector<Unpack>& out, std::string& err) {
    size_t i = 0;
    while (i + 4 <= v.size()) {
        const uint32_t code = rd<uint32_t>(v, i);
        i += 4;
        const uint32_t cmd = (code >> 24) & 0x7f;
        if (cmd == 0) continue;  // NOP
        if (cmd < 0x60) {
            char b[64];
            std::snprintf(b, sizeof b, "VIF code %08x at +%zx", code, i - 4);
            err = b;
            return false;
        }
        Unpack u;
        u.vn = (cmd >> 2) & 3;
        u.vl = cmd & 3;
        u.masked = (cmd & 0x10) != 0;
        u.num = ((code >> 16) & 0xff) == 0 ? 256 : (code >> 16) & 0xff;
        u.addr = code & 0x3ff;
        u.usn = (code & 0x4000) != 0;
        u.tops = (code & 0x8000) != 0;
        const uint32_t bits = (u.vn == 3 && u.vl == 3) ? 16 : (32u >> u.vl) * (u.vn + 1);
        u.bytes = ((u.num * bits + 31) / 32) * 4;
        u.data = i;
        if (i + u.bytes > v.size()) {
            err = "UNPACK past the list's end";
            return false;
        }
        i += u.bytes;
        out.push_back(u);
    }
    return true;
}

}  // namespace

bool decode_class(std::span<const uint8_t> cls, uint32_t base, MobyClass& c, std::string& err, std::span<const uint8_t> tex_table) {
    if (cls.size() < 0x48) {
        err = "class shorter than its header";
        return false;
    }
    c.header.assign(cls.begin(), cls.begin() + 0x48);
    const uint32_t pt = rd<uint32_t>(cls, 0);
    c.lod_count[0] = cls[4];
    c.lod_count[1] = cls[5];
    c.shadow_count = cls[6];
    c.shadow_first = cls[7];
    c.joints_hi = cls[8];
    c.joints_lo = cls[9];
    c.tint_first[0] = cls[0xa];
    c.tint_first[1] = cls[0xb];
    c.scale = rd<float>(cls, 0x24);
    for (int k = 0; k < 4; ++k) c.bsphere[static_cast<size_t>(k)] = rd<float>(cls, 0x30 + 4 * static_cast<size_t>(k));
    // the skeleton (the joint count is the class's +8)
    c.joint_count = cls[8];
    const uint32_t skel = rd<uint32_t>(cls, 0x14), trans = rd<uint32_t>(cls, 0x18);
    if (skel != 0)
        for (uint32_t j = 0; j < c.joint_count; ++j) {
            std::array<float, 16> m{};
            for (size_t k = 0; k < 16; ++k) m[k] = rd<float>(cls, off(skel, base) + j * 0x40 + 4 * k);
            c.inverse_bind.push_back(m);
        }
    if (trans != 0)
        for (uint32_t j = 0; j < c.joint_count; ++j) {
            std::array<uint8_t, 16> t{};
            for (size_t k = 0; k < 16; ++k) t[k] = rd<uint8_t>(cls, off(trans, base) + j * 0x10 + k);
            c.joint_binds.push_back(t);
        }
    if (pt == 0) return true;  // no geometry
    const uint32_t ptab = off(pt, base);
    // every packet any draw uses: the two LODs, the shadow mesh
    uint32_t n_packets = static_cast<uint32_t>(c.lod_count[0]) + c.lod_count[1];
    if (c.shadow_count != 0) n_packets = std::max<uint32_t>(n_packets, static_cast<uint32_t>(c.shadow_first) + c.shadow_count);
    // the GIF usage table (+0x20): per texture prim block, the level texture ids its prims use
    std::map<uint32_t, std::vector<uint8_t>> usage;  // prim block offset -> ids
    if (const uint32_t gu = rd<uint32_t>(cls, 0x20); gu != 0) {
        for (uint32_t e = off(gu, base);; e += 0x10) {
            if (e + 0x10 > cls.size()) break;
            std::vector<uint8_t> ids;
            for (uint32_t k = 0; k < 12 && cls[e + k] != 0xff; ++k) ids.push_back(cls[e + k]);
            const uint32_t p = rd<uint32_t>(cls, e + 0xc);
            usage[off(p & 0x7fffffffu, base)] = ids;
            if ((p & 0x80000000u) != 0) break;
        }
    }
    // the VU0 vertex cache's writers, per LOD, to resolve the duplicates
    std::map<int, std::array<std::array<int32_t, 2>, 512>> cache_of;
    std::map<int, std::array<bool, 256>> vu0_written;
    for (uint32_t k = 0; k < n_packets; ++k) {
        Packet p;
        const uint32_t e = ptab + k * 0x10;
        if (e + 0x10 > cls.size()) {
            err = "packet table past the class";
            return false;
        }
        std::memcpy(p.entry, cls.data() + e, 16);
        const uint32_t vif = off(rd<uint32_t>(cls, e), base), vqwc = rd<uint16_t>(cls, e + 4);
        const uint32_t vd = off(rd<uint32_t>(cls, e + 8), base);
        const uint32_t vsize = cls[e + 0xc] * 16u;
        // the entry as offsets
        const uint32_t ov = vif, od = vd;
        le_store(p.entry, ov);
        le_store(p.entry + 8, od);
        if (k < c.lod_count[0]) {
            p.lod = 0;
            p.index_in_lod = static_cast<uint8_t>(k);
        } else if (k < static_cast<uint32_t>(c.lod_count[0]) + c.lod_count[1]) {
            p.lod = 1;
            p.index_in_lod = static_cast<uint8_t>(k - c.lod_count[0]);
        } else {
            p.lod = 2;
            p.index_in_lod = static_cast<uint8_t>(k - c.shadow_first);
        }
        if (vif + vqwc * 16u > cls.size() || vd + vsize > cls.size()) {
            err = "packet data past the class";
            return false;
        }
        p.vif.assign(cls.begin() + vif, cls.begin() + vif + vqwc * 16u);
        p.vdata.assign(cls.begin() + vd, cls.begin() + vd + vsize);
        p.n_slots = cls[e + 0xf];
        // ---- the VIF list
        std::vector<Unpack> ups;
        if (!parse_vif(p.vif, ups, err)) return false;
        const std::span<const uint8_t> vs(p.vif);
        const Unpack* ui = nullptr;
        for (const Unpack& u : ups) {
            if (u.vn == 1 && u.vl == 1) {  // V2_16: the UVs
                p.vif_uv_addr = static_cast<uint16_t>(u.addr);
                p.uv.resize(u.num);
                for (uint32_t s = 0; s < u.num; ++s) p.uv[s] = {rd<int16_t>(vs, u.data + 4 * s), rd<int16_t>(vs, u.data + 4 * s + 2)};
            } else if (u.vn == 3 && u.vl == 2) {  // V4_8: the index list
                ui = &u;
                p.vif_index_addr = static_cast<uint16_t>(u.addr);
                for (int b = 0; b < 4; ++b) p.index_header[b] = rd<int8_t>(vs, u.data + static_cast<size_t>(b));
                for (uint32_t b = 4; b < u.num * 4; ++b) p.indices.push_back(rd<int8_t>(vs, u.data + b));
            } else if (u.vn == 3 && u.vl == 0) {  // V4_32: the texture prims (4 qwords each)
                p.vif_tex_addr = static_cast<uint16_t>(u.addr);
                p.vif_tex_off = static_cast<uint32_t>(u.data);
                const std::vector<uint8_t>* ids = nullptr;
                // (the usage table points at the qword before the prims: their UNPACK code)
                if (auto it = usage.find(vif + static_cast<uint32_t>(u.data) - 0x10); it != usage.end()) ids = &it->second;
                for (uint32_t t = 0; t + 4 <= u.num; t += 4) {
                    TexturePrim tp;
                    for (int r = 0; r < 4; ++r) {
                        const size_t q = u.data + (t + static_cast<uint32_t>(r)) * 16;
                        tp.file[r] = rd<uint64_t>(vs, q);
                        const float a = rd<float>(vs, q + 8);
                        tp.reg[r] = static_cast<uint8_t>(static_cast<int32_t>(a));  // (stored as a float: the VU1 program's FTOI)
                    }
                    tp.next_secret = rd<int8_t>(vs, u.data + t * 16 + 0xc);
                    tp.local_texture = static_cast<int32_t>(static_cast<uint32_t>(tp.file[2]));
                    // 0x2406b0: ti = remap[ti] when ti >= 0 (lbu of the 16-byte remap: the low byte indexes it)
                    tp.texture = tp.local_texture >= 0 ? c.texture_remap[static_cast<size_t>(tp.local_texture) & 0xf] : tp.local_texture;
                    if (tp.local_texture > 15) {
                        err = "texture prim's local texture past the class remap";
                        return false;
                    }
                    // the class's GIF usage table names the same texture (its bytes the remap's input)
                    const size_t ti = t / 4;
                    if (ids != nullptr && ti < ids->size() && tp.local_texture >= 0 && (*ids)[ti] != tp.local_texture) {
                        err = "GIF usage table disagrees with the texture prim";
                        return false;
                    }
                    build_prim_gs(tp, tex_table);
                    p.textures.push_back(tp);
                }
            } else {
                char b[64];
                std::snprintf(b, sizeof b, "unexpected UNPACK vn %u vl %u", u.vn, u.vl);
                err = b;
                return false;
            }
        }
        // ---- the strip as the VU1 program walks it (the index list in quadwords of four)
        if (ui != nullptr) {
            int8_t secret = p.index_header[2];
            size_t prim = 0;
            bool ended = false;
            for (size_t j = 0; j < p.indices.size(); ++j) {
                const int8_t ix = p.indices[j];
                StripOp op;
                if (ix > 0) {
                    op.slot = static_cast<uint8_t>(ix);
                } else if (ix < 0) {
                    op.slot = static_cast<uint8_t>(ix + 128);
                    op.adc = true;
                } else {
                    if (secret == 0) {
                        ended = true;
                        break;
                    }
                    op.slot = static_cast<uint8_t>(secret);
                    op.texture = static_cast<int16_t>(prim);
                    secret = prim < p.textures.size() ? p.textures[prim].next_secret : 0;
                    ++prim;
                }
                p.strip.push_back(op);
            }
            if (!ended) {
                err = "index list without its end";
                return false;
            }
        }
        // ---- the vertex data (the skinning's input). The shadow mesh's packets (class +6 / +7,
        // drawn flattened along the light by the draw loop's 0x256b50) have another layout, skinned
        // by the second half of 0x1ee858 (vcallmsr 0x111 / 0x121 / 0x12d): kept raw, not decoded.
        if (p.lod == 2) {
            c.packets.push_back(std::move(p));
            continue;
        }
        const std::span<const uint8_t> vb(p.vdata);
        const uint32_t n_mt = rd<uint32_t>(vb, 0), n2 = rd<uint32_t>(vb, 4), n3 = rd<uint32_t>(vb, 8), nm = rd<uint32_t>(vb, 0xc);
        const uint32_t ndup = rd<uint32_t>(vb, 0x10), ntr = rd<uint32_t>(vb, 0x14), vto = rd<uint32_t>(vb, 0x18), col = rd<uint32_t>(vb, 0x1c);
        const uint32_t n = n2 + n3 + nm;
        if (ntr != p.n_slots || n + ndup != ntr) {
            char b[96];
            std::snprintf(b, sizeof b, "packet %u: slots %u, transfer %u, records %u + duplicates %u", k, p.n_slots, ntr, n, ndup);
            err = b;
            return false;
        }
        for (uint32_t m = 0; m < n_mt; ++m) p.matrix_transfers.push_back({vb[0x20 + 2 * m], vb[0x21 + 2 * m]});
        const uint32_t dup_at = 0x20 + ((2 * n_mt + 6) & ~7u);
        for (uint32_t d = 0; d < ndup; ++d) p.duplicates.push_back(static_cast<uint16_t>(rd<uint16_t>(vb, dup_at + 2 * d) >> 7));
        p.vertices.resize(n);
        for (uint32_t v = 0; v < n; ++v) {
            SkinVertex& sv = p.vertices[v];
            const size_t r = vto + 16 * v;
            std::memcpy(sv.raw, vb.data() + std::min<size_t>(r, vb.size() - 16), 16);
            const uint16_t h = rd<uint16_t>(vb, r);
            sv.joint = static_cast<uint8_t>(h >> 9);
            const uint8_t* b = sv.raw;
            if (v < n2) {
                sv.kind = VertexKind::TwoWay;
                sv.src[0] = b[2];
                sv.src[1] = b[3];
                sv.weight[0] = b[4];
                sv.weight[1] = b[5];
                sv.store = b[6];
                sv.blend_store = b[7];
            } else if (v < n2 + n3) {
                sv.kind = VertexKind::ThreeWay;
                sv.src[0] = b[2];
                sv.src[1] = b[3];
                sv.src[2] = static_cast<uint8_t>((h >> 8) & 0xfe);
                sv.weight[0] = b[4];
                sv.weight[1] = b[5];
                sv.weight[2] = b[6];
                sv.blend_store = b[7];
            } else {
                sv.kind = VertexKind::Single;
                sv.src[0] = b[2];
                sv.store = b[3];
            }
            sv.normal[0] = b[8];
            sv.normal[1] = b[9];
            for (int a = 0; a < 3; ++a) sv.pos[a] = rd<int16_t>(vb, r + 10 + 2 * static_cast<size_t>(a));
            for (int a = 0; a < 4; ++a) sv.rgba[a] = rd<uint8_t>(vb, col + 4 * v + static_cast<size_t>(a));
            // the vertex-cache index: in the low 9 bits of the record 7 places later (the
            // skinning's pipeline), the last 7 in the quadwords after the records
            if (v + 7 < n) sv.cache_index = static_cast<uint16_t>(rd<uint16_t>(vb, vto + 16 * (v + 7)) & 0x1ff);
        }
        if (n > 0) {
            // the tail (see the EE's 0x1ef184): the extra quadwords between the records and the
            // colours; the first ones hold an index in their first halfword like a record, the
            // last one six in its halfwords 2..7
            const int ep = static_cast<int>((col - vto) / 16) - static_cast<int>(n);
            const int first = std::max(7 - static_cast<int>(n), 0);
            size_t q = vto + 16 * n + 16 * static_cast<size_t>(first);
            for (int k2 = first; k2 < ep; ++k2, q += 16) {
                const int dest = static_cast<int>(n) + k2 - 7;
                if (dest >= 0 && dest < static_cast<int>(n)) p.vertices[static_cast<size_t>(dest)].cache_index = static_cast<uint16_t>(rd<uint16_t>(vb, q) & 0x1ff);
            }
            if (ep > 0) {
                const size_t last = vto + 16 * (n + static_cast<size_t>(ep) - 1);
                const int start = std::max(7 - static_cast<int>(n) - ep, 0);
                for (int k2 = start; k2 < 6; ++k2) {
                    const int dest = static_cast<int>(n) + ep + k2 - 7;
                    if (dest >= 0 && dest < static_cast<int>(n))
                        p.vertices[static_cast<size_t>(dest)].cache_index = static_cast<uint16_t>(rd<uint16_t>(vb, last + 4 + 2 * static_cast<size_t>(k2)) & 0x1ff);
                }
            }
        }
        // duplicates through the cache of this LOD's packets so far
        auto& cache = cache_of.try_emplace(p.lod).first->second;
        if (p.index_in_lod == 0)
            for (auto& x : cache) x = {-1, -1};
        for (uint32_t v = 0; v < n; ++v) cache[p.vertices[v].cache_index] = {static_cast<int32_t>(k), static_cast<int32_t>(v)};
        for (uint16_t d : p.duplicates) p.duplicate_source.push_back(cache[d & 0x1ff]);
        // the VU0 matrix addresses read before this LOD's packets wrote them (another moby's
        // matrices: the VU0 memory is not cleared between mobys)
        auto& written = vu0_written.try_emplace(p.lod).first->second;
        if (p.index_in_lod == 0) written.fill(false);
        auto wr = [&](uint8_t a) {
            for (int r = 0; r < 4; ++r) written[static_cast<uint8_t>(a + r)] = true;
        };
        auto rd4 = [&](uint8_t a) {
            for (int r = 0; r < 4; ++r)
                if (!written[static_cast<uint8_t>(a + r)]) return false;
            return true;
        };
        for (const auto& mt : p.matrix_transfers) wr(mt[1]);
        for (const SkinVertex& sv : p.vertices) {
            if (sv.kind != VertexKind::ThreeWay) wr(sv.store);
            const int ns = sv.kind == VertexKind::Single ? 1 : sv.kind == VertexKind::TwoWay ? 2 : 3;
            for (int s2 = 0; s2 < ns; ++s2)
                if (!rd4(sv.src[s2])) ++p.stale_matrix_reads;
            if (sv.kind != VertexKind::Single) wr(sv.blend_store);
        }
        c.packets.push_back(std::move(p));
    }
    return true;
}

// orig: (host) the GS registers 0x2400f0 builds at the level load, `vram` = *(0x15ee8c) >> 8
// (idx < 0: its special forms; the chrome / glass registers (-2 / -3) are the ELF's data at
// 0x1827c0 / 0x1827d8: not built here, false)
bool prim_gs(const TexturePrim& t, int32_t vram, uint64_t gs[4]) {
    const uint64_t a = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t.file[0])));
    const uint64_t b = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t.file[0] >> 32)));
    const uint64_t c = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t.file[1])));
    const uint64_t d = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(t.file[1] >> 32)));
    const int64_t idx = t.texture;
    auto U = [](int64_t v) { return static_cast<uint64_t>(v); };
    if (idx >= 0) {
        if (!t.has_entry) return false;
        const std::span<const uint8_t> te(t.tex_entry);
        auto h = [&](size_t o) { return static_cast<int32_t>(rd<int16_t>(te, o)); };
        auto hbi = [](int32_t v) {  // 0x1feff8
            const uint32_t u = static_cast<uint32_t>(v), sign = u >> 31;
            int32_t n = 0;
            for (int i = 30; i >= 0 && ((u >> i) & 1u) == sign; --i) ++n;
            return 30 - n;
        };
        const int32_t w = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(h(4))) << 16);
        int32_t tw = w >> 0x17, th = w >> 0x16;
        if (!(0 < tw)) tw = 1;
        if (!(0 < th)) th = 1;
        const int32_t lw2 = hbi(w >> 16), lh2 = hbi(h(6));
        auto add = [vram](int32_t x) { return static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(x) + static_cast<uint32_t>(vram))); };
        const int64_t ee = add(h(0xe)), f = add(h(0xa)), hh = add(h(0xc)), s8 = h(8);
        gs[0] = (U(s8 - 1) << 2) | ((b << 6) | 0x20u) | (a << 32);
        gs[1] = c | (d << 2) | (U(idx) << 24);
        gs[2] = (U(th) << 14) | ((U(lw2) << 26) | 0x01300000u) | (U(lh2) << 30) | ((U(f) << 37) | 0x400000000ull) | 0x8000000000000000ull;
        gs[3] = (U(tw) << 14) | (U(hh) << 20) | ((U(ee) << 40) | 0x400000000ull) | 0x0040000000000000ull;
        return true;
    }
    gs[0] = (b << 6) | ((a << 32) | 0x20u);
    gs[1] = 5;
    if (idx < -1) return false;
    gs[2] = 0x80000004cc007ffbull;
    gs[3] = 0;
    return true;
}

void build_prim_gs(TexturePrim& t, std::span<const uint8_t> tex_table) {
    t.has_entry = false;
    if (t.texture >= 0 && static_cast<size_t>(t.texture) * 16 + 16 <= tex_table.size()) {
        std::memcpy(t.tex_entry, tex_table.data() + static_cast<size_t>(t.texture) * 16, 16);
        t.has_entry = true;
    }
    t.gs_known = prim_gs(t, 0, t.gs);
}

// ---- file format (little-endian) ----

namespace {

// (every number little-endian, an array's one by one: le_fix_elements; the palette's words are bytes: raw)
struct Out {
    std::vector<uint8_t> b;
    template <typename T> void put(const T& v0) {
        T v;  // (a C array too)
        std::memcpy(&v, &v0, sizeof(T));
        le_fix_elements(v);
        put_raw(v);
    }
    template <typename T> void put_raw(const T& v) {
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        b.insert(b.end(), p, p + sizeof(T));
    }
    void bytes(const std::vector<uint8_t>& v) {
        put(static_cast<uint32_t>(v.size()));
        b.insert(b.end(), v.begin(), v.end());
    }
};
struct In {
    std::vector<uint8_t> b;
    size_t at = 0;
    bool ok = true;
    template <typename T> T get() {
        T v = get_raw<T>();
        le_fix_elements(v);
        return v;
    }
    template <typename T> T get_raw() {
        T v{};
        if (at + sizeof(T) > b.size()) {
            ok = false;
            return v;
        }
        std::memcpy(&v, b.data() + at, sizeof(T));
        at += sizeof(T);
        return v;
    }
    std::vector<uint8_t> bytes() {
        const uint32_t n = get<uint32_t>();
        if (!ok || at + n > b.size()) {
            ok = false;
            return {};
        }
        std::vector<uint8_t> v(b.begin() + static_cast<long>(at), b.begin() + static_cast<long>(at + n));
        at += n;
        return v;
    }
};

bool save(const std::string& path, const std::vector<uint8_t>& b) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size();
    return std::fclose(f) == 0 && ok;
}
bool load(const std::string& path, std::vector<uint8_t>& b) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    b.resize(static_cast<size_t>(std::max(n, 0L)));
    const bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
    std::fclose(f);
    return ok;
}

}  // namespace

bool write_class(const std::string& path, const MobyClass& c) {
    Out o;
    o.put(kMobyMagic);
    o.put(kMobyVersion);
    o.put(c.level);
    o.put(c.o_class);
    o.put(static_cast<uint32_t>(c.gadget ? 1 : 0));
    o.bytes(c.header);
    o.put(c.lod_count);
    o.put(c.shadow_count);
    o.put(c.shadow_first);
    o.put(c.joints_hi);
    o.put(c.joints_lo);
    o.put(c.tint_first);
    o.put(c.scale);
    o.put(c.bsphere);
    o.put(c.texture_remap);
    o.put(c.joint_count);
    o.put(static_cast<uint32_t>(c.inverse_bind.size()));
    for (const auto& m : c.inverse_bind) o.put(m);
    o.put(static_cast<uint32_t>(c.joint_binds.size()));
    for (const auto& t : c.joint_binds) o.put(t);
    o.put(static_cast<uint32_t>(c.packets.size()));
    for (const Packet& p : c.packets) {
        o.put(p.lod);
        o.put(p.index_in_lod);
        o.put(p.entry);
        o.bytes(p.vif);
        o.bytes(p.vdata);
        o.put(p.n_slots);
        o.put(static_cast<uint32_t>(p.uv.size()));
        for (const auto& u : p.uv) o.put(u);
        o.put(p.index_header);
        o.put(static_cast<uint32_t>(p.indices.size()));
        for (int8_t x : p.indices) o.put(x);
        o.put(static_cast<uint32_t>(p.textures.size()));
        for (const TexturePrim& t : p.textures) {
            o.put(t.file);
            o.put(t.reg);
            o.put(t.next_secret);
            o.put(t.local_texture);
            o.put(t.texture);
            o.put(t.gs);
            o.put(static_cast<uint8_t>(t.gs_known ? 1 : 0));
            o.put(t.tex_entry);
            o.put(static_cast<uint8_t>(t.has_entry ? 1 : 0));
        }
        o.put(static_cast<uint32_t>(p.strip.size()));
        for (const StripOp& s : p.strip) {
            o.put(s.slot);
            o.put(static_cast<uint8_t>(s.adc ? 1 : 0));
            o.put(s.texture);
        }
        o.put(static_cast<uint32_t>(p.kicks.size()));
        for (const KickOp& k : p.kicks) {
            o.put(static_cast<uint8_t>(k.kind));
            o.put(k.pos);
            o.put(k.pos_q);
            o.put(k.col);
            o.put(k.uv);
            o.put(k.uv_q);
            o.put(k.uv_q2);
            o.put(static_cast<uint8_t>(k.adc ? 1 : 0));
            o.put(k.texture);
        }
        o.put(static_cast<uint32_t>(p.kicks_clip.size()));
        for (const KickOp& k : p.kicks_clip) {
            o.put(static_cast<uint8_t>(k.kind));
            o.put(k.pos);
            o.put(k.pos_q);
            o.put(k.col);
            o.put(k.uv);
            o.put(k.uv_q);
            o.put(k.uv_q2);
            o.put(static_cast<uint8_t>(k.adc ? 1 : 0));
            o.put(k.texture);
        }
        o.put(static_cast<uint8_t>(p.kicks_ok ? 1 : 0));
        o.put(static_cast<uint32_t>(p.kicks_note.size()));
        o.b.insert(o.b.end(), p.kicks_note.begin(), p.kicks_note.end());
        o.put(p.vif_uv_addr);
        o.put(p.vif_index_addr);
        o.put(p.vif_tex_addr);
        o.put(p.vif_tex_off);
        o.put(p.stale_matrix_reads);
        o.put(static_cast<uint32_t>(p.matrix_transfers.size()));
        for (const auto& m : p.matrix_transfers) o.put(m);
        o.put(static_cast<uint32_t>(p.vertices.size()));
        for (const SkinVertex& v : p.vertices) {
            o.put(static_cast<uint8_t>(v.kind));
            o.put(v.joint);
            o.put(v.cache_index);
            o.put(v.src);
            o.put(v.weight);
            o.put(v.store);
            o.put(v.blend_store);
            o.put(v.normal);
            o.put(v.pos);
            o.put(v.rgba);
            o.put(v.raw);
        }
        o.put(static_cast<uint32_t>(p.duplicates.size()));
        for (size_t d = 0; d < p.duplicates.size(); ++d) {
            o.put(p.duplicates[d]);
            o.put(p.duplicate_source[d]);
        }
    }
    return save(path, o.b);
}

bool read_class(const std::string& path, MobyClass& c) {
    In in;
    if (!load(path, in.b)) return false;
    if (in.get<uint32_t>() != kMobyMagic || in.get<uint32_t>() != kMobyVersion) return false;
    c.level = in.get<int32_t>();
    c.o_class = in.get<int32_t>();
    c.gadget = in.get<uint32_t>() != 0;
    c.header = in.bytes();
    for (auto& x : c.lod_count) x = in.get<uint8_t>();
    c.shadow_count = in.get<uint8_t>();
    c.shadow_first = in.get<uint8_t>();
    c.joints_hi = in.get<uint8_t>();
    c.joints_lo = in.get<uint8_t>();
    for (auto& x : c.tint_first) x = in.get<uint8_t>();
    c.scale = in.get<float>();
    c.bsphere = in.get<std::array<float, 4>>();
    c.texture_remap = in.get<std::array<uint8_t, 16>>();
    c.joint_count = in.get<uint32_t>();
    c.inverse_bind.resize(in.get<uint32_t>());
    for (auto& m : c.inverse_bind) m = in.get<std::array<float, 16>>();
    c.joint_binds.resize(in.get<uint32_t>());
    for (auto& t : c.joint_binds) t = in.get<std::array<uint8_t, 16>>();
    const uint32_t np = in.get<uint32_t>();
    if (!in.ok || np > 4096) return false;
    c.packets.resize(np);
    for (Packet& p : c.packets) {
        p.lod = in.get<uint8_t>();
        p.index_in_lod = in.get<uint8_t>();
        for (auto& x : p.entry) x = in.get<uint8_t>();
        p.vif = in.bytes();
        p.vdata = in.bytes();
        p.n_slots = in.get<uint32_t>();
        p.uv.resize(in.get<uint32_t>());
        for (auto& u : p.uv) u = in.get<std::array<int16_t, 2>>();
        for (auto& x : p.index_header) x = in.get<int8_t>();
        p.indices.resize(in.get<uint32_t>());
        for (auto& x : p.indices) x = in.get<int8_t>();
        p.textures.resize(in.get<uint32_t>());
        for (TexturePrim& t : p.textures) {
            for (auto& x : t.file) x = in.get<uint64_t>();
            for (auto& x : t.reg) x = in.get<uint8_t>();
            t.next_secret = in.get<int8_t>();
            t.local_texture = in.get<int32_t>();
            t.texture = in.get<int32_t>();
            for (auto& x : t.gs) x = in.get<uint64_t>();
            t.gs_known = in.get<uint8_t>() != 0;
            for (auto& x : t.tex_entry) x = in.get<uint8_t>();
            t.has_entry = in.get<uint8_t>() != 0;
        }
        p.strip.resize(in.get<uint32_t>());
        for (StripOp& s : p.strip) {
            s.slot = in.get<uint8_t>();
            s.adc = in.get<uint8_t>() != 0;
            s.texture = in.get<int16_t>();
        }
        p.kicks.resize(in.get<uint32_t>());
        for (KickOp& k : p.kicks) {
            k.kind = static_cast<KickKind>(in.get<uint8_t>());
            k.pos = in.get<uint8_t>();
            k.pos_q = in.get<uint8_t>();
            k.col = in.get<uint8_t>();
            k.uv = in.get<uint8_t>();
            k.uv_q = in.get<uint8_t>();
            k.uv_q2 = in.get<uint8_t>();
            k.adc = in.get<uint8_t>() != 0;
            k.texture = in.get<int16_t>();
        }
        p.kicks_clip.resize(in.get<uint32_t>());
        for (KickOp& k : p.kicks_clip) {
            k.kind = static_cast<KickKind>(in.get<uint8_t>());
            k.pos = in.get<uint8_t>();
            k.pos_q = in.get<uint8_t>();
            k.col = in.get<uint8_t>();
            k.uv = in.get<uint8_t>();
            k.uv_q = in.get<uint8_t>();
            k.uv_q2 = in.get<uint8_t>();
            k.adc = in.get<uint8_t>() != 0;
            k.texture = in.get<int16_t>();
        }
        p.kicks_ok = in.get<uint8_t>() != 0;
        {
            const uint32_t n = in.get<uint32_t>();
            for (uint32_t i = 0; i < n && in.ok; ++i) p.kicks_note.push_back(static_cast<char>(in.get<uint8_t>()));
        }
        p.vif_uv_addr = in.get<uint16_t>();
        p.vif_index_addr = in.get<uint16_t>();
        p.vif_tex_addr = in.get<uint16_t>();
        p.vif_tex_off = in.get<uint32_t>();
        p.stale_matrix_reads = in.get<uint32_t>();
        p.matrix_transfers.resize(in.get<uint32_t>());
        for (auto& m : p.matrix_transfers) m = in.get<std::array<uint8_t, 2>>();
        p.vertices.resize(in.get<uint32_t>());
        for (SkinVertex& v : p.vertices) {
            v.kind = static_cast<VertexKind>(in.get<uint8_t>());
            v.joint = in.get<uint8_t>();
            v.cache_index = in.get<uint16_t>();
            for (auto& x : v.src) x = in.get<uint8_t>();
            for (auto& x : v.weight) x = in.get<uint8_t>();
            v.store = in.get<uint8_t>();
            v.blend_store = in.get<uint8_t>();
            for (auto& x : v.normal) x = in.get<uint8_t>();
            for (auto& x : v.pos) x = in.get<int16_t>();
            for (auto& x : v.rgba) x = in.get<uint8_t>();
            for (auto& x : v.raw) x = in.get<uint8_t>();
        }
        const uint32_t nd = in.get<uint32_t>();
        if (!in.ok || nd > 4096) return false;
        p.duplicates.resize(nd);
        p.duplicate_source.resize(nd);
        for (uint32_t d = 0; d < nd; ++d) {
            p.duplicates[d] = in.get<uint16_t>();
            p.duplicate_source[d] = in.get<std::array<int32_t, 2>>();
        }
    }
    return in.ok;
}

bool write_textures(const std::string& path, const std::vector<Texture>& t) {
    Out o;
    o.put(kTexMagic);
    o.put(kMobyVersion);
    o.put(static_cast<uint32_t>(t.size()));
    for (const Texture& x : t) {
        o.put(x.index);
        o.put(x.width);
        o.put(x.height);
        o.put_raw(x.palette);  // (the palette's RGBA bytes as stored: their order in memory, not words)
        o.put(static_cast<uint32_t>(x.mips.size()));
        for (const auto& m : x.mips) o.bytes(m);
    }
    return save(path, o.b);
}

bool read_textures(const std::string& path, std::vector<Texture>& t) {
    In in;
    if (!load(path, in.b)) return false;
    if (in.get<uint32_t>() != kTexMagic || in.get<uint32_t>() != kMobyVersion) return false;
    t.resize(in.get<uint32_t>());
    for (Texture& x : t) {
        x.index = in.get<uint32_t>();
        x.width = in.get<uint32_t>();
        x.height = in.get<uint32_t>();
        x.palette = in.get_raw<std::array<uint32_t, 256>>();  // (RGBA bytes: kept in their file order, as the texture uploads read them)
        x.mips.resize(in.get<uint32_t>());
        for (auto& m : x.mips) m = in.bytes();
    }
    return in.ok;
}

// ---- the CPU reference of the skinning ----

void skin_packet(const Packet& p, const SkinInput& in, SkinState& st, std::vector<std::array<int16_t, 3>>& pos,
                 std::vector<std::array<uint8_t, 4>>& rgba) {
    auto store = [&](uint8_t at, const float* rows) {
        for (int r = 0; r < 4; ++r)
            for (int l = 0; l < 4; ++l) st.vu0[static_cast<uint8_t>(at + r)][static_cast<size_t>(l)] = rows[4 * r + l];
    };
    auto joint = [&](uint32_t j) -> const float* {
        static const float zero[16] = {};
        return j < in.joint_count ? in.joints[j] : zero;
    };
    for (const auto& mt : p.matrix_transfers) store(mt[1], joint(mt[0]));
    pos.assign(p.n_slots, {0, 0, 0});
    rgba.assign(p.n_slots, {0, 0, 0, 0});
    for (size_t v = 0; v < p.vertices.size(); ++v) {
        const SkinVertex& sv = p.vertices[v];
        float m[16];
        auto row = [&](uint8_t at, int r, int l) { return st.vu0[static_cast<uint8_t>(at + r)][static_cast<size_t>(l)]; };
        if (sv.kind == VertexKind::Single) {
            store(sv.store, joint(sv.joint));
            for (int r = 0; r < 4; ++r)
                for (int l = 0; l < 4; ++l) m[4 * r + l] = row(sv.src[0], r, l);
        } else {
            if (sv.kind == VertexKind::TwoWay) store(sv.store, joint(sv.joint));
            const int nw = sv.kind == VertexKind::TwoWay ? 2 : 3;
            float w[3];
            for (int k = 0; k < nw; ++k) w[k] = static_cast<float>(sv.weight[k]) * (1.0f / 4096.0f) * 16.0f;
            for (int r = 0; r < 4; ++r)
                for (int l = 0; l < 4; ++l) {
                    float a = row(sv.src[0], r, l) * w[0];
                    a = a + row(sv.src[1], r, l) * w[1];
                    if (nw == 3) a = a + row(sv.src[2], r, l) * w[2];
                    m[4 * r + l] = a;
                }
            float blended[16];
            std::memcpy(blended, m, sizeof m);
            store(sv.blend_store, blended);
        }
        // the position: rows 0..2 by x, y, z plus row 3, truncated to integers
        float o[3];
        for (int l = 0; l < 3; ++l) {
            float a = m[0 + l] * static_cast<float>(sv.pos[0]);
            a = a + m[4 + l] * static_cast<float>(sv.pos[1]);
            a = a + m[8 + l] * static_cast<float>(sv.pos[2]);
            o[l] = a + m[12 + l];
        }
        std::array<int16_t, 3> q{};
        for (int l = 0; l < 3; ++l) q[static_cast<size_t>(l)] = static_cast<int16_t>(static_cast<int32_t>(o[l]));
        pos[v] = q;
        // the lighting (the VU0 program's second half of each vertex): the normal from the table
        // (n = (a.x * e.x, a.y * e.x, e.y), a = table[normal[0]], e = table[normal[1]]) by the
        // matrix's rows 0..2; per light i (lane i of the rows L0..L2) d = L . n', max(d, d * k)
        // (k = -|row 6|); the colour rows 3..5 weighted by them, over |n'|, plus row 7 (the moby
        // colour as 65536 + byte); the EE keeps the float's low halfword (the value x 128),
        // multiplies it by the vertex colour (0x80 = 1), saturates to 16 bits, >> 7, low byte.
        {
            static const float zero2[2] = {0.0f, 0.0f};
            const float* ta = in.normal_table != nullptr ? in.normal_table[sv.normal[0]] : zero2;
            const float* te = in.normal_table != nullptr ? in.normal_table[sv.normal[1]] : zero2;
            const float nx = ta[0] * te[0], ny = ta[1] * te[0], nw = te[1];
            float n[4];
            for (int l = 0; l < 4; ++l) {
                float a = m[0 + l] * nx;
                a = a + m[4 + l] * ny;
                n[l] = a + m[8 + l] * nw;
            }
            float len2 = n[0] * n[0] + n[1] * n[1];
            len2 = len2 + n[2] * n[2];
            // (the VU's RSQRT of 0 is the largest float, and 0 x it is 0: a zero normal gets row 7 only)
            const float qn = len2 > 0.0f ? 1.0f / std::sqrt(len2) : 3.4028235e38f;
            float d[4], c[4];
            for (int l = 0; l < 4; ++l) {
                float a = in.light[0][static_cast<size_t>(l)] * n[0];
                a = a + in.light[1][static_cast<size_t>(l)] * n[1];
                d[l] = a + in.light[2][static_cast<size_t>(l)] * n[2];
                const float k = -std::fabs(in.light[6][static_cast<size_t>(l)]);
                d[l] = std::max(d[l], d[l] * k);
            }
            for (int l = 0; l < 4; ++l) {
                float a = in.light[3][static_cast<size_t>(l)] * d[0];
                a = a + in.light[4][static_cast<size_t>(l)] * d[1];
                a = a + in.light[5][static_cast<size_t>(l)] * d[2];
                c[l] = (a == 0.0f ? 0.0f : a * qn) + in.light[7][static_cast<size_t>(l)] * 1.0f;
            }
            std::array<uint8_t, 4> out{};
            for (int l = 0; l < 4; ++l) {
                uint32_t bits;
                std::memcpy(&bits, &c[l], 4);
                const int32_t h = static_cast<int16_t>(bits & 0xffff);
                int32_t pr = h * static_cast<int32_t>(sv.rgba[l]);
                pr = std::clamp(pr, -32768, 32767);
                out[static_cast<size_t>(l)] = static_cast<uint8_t>((pr >> 7) & 0xff);
            }
            rgba[v] = out;
        }
        st.cache_pos[sv.cache_index] = q;
        st.cache_rgba[sv.cache_index] = rgba[v];
    }
    for (size_t d = 0; d < p.duplicates.size(); ++d) {
        const size_t s = p.vertices.size() + d;
        if (s >= pos.size()) break;
        pos[s] = st.cache_pos[p.duplicates[d] & 0x1ff];
        rgba[s] = st.cache_rgba[p.duplicates[d] & 0x1ff];
    }
}

// ---- the CPU reference of the VU1 program (micro 0x22d, the no-clip form: moby_vu1.cpp) ----

namespace {
// FTOI4: x 16 to an integer, saturated as the VU does (no undefined float-to-int conversion)
uint32_t ftoi4(float f) {
    const float v = f * 16.0f;
    if (!(v < 2147483648.0f)) return std::isnan(v) ? 0x7fffffffu : 0x7fffffffu;
    if (!(v > -2147483648.0f)) return 0x80000000u;
    return static_cast<uint32_t>(static_cast<int32_t>(v));
}
}  // namespace

GsVertex vu1_vertex(const Vu1Consts& k, const int32_t pos[3], const int32_t rgba[4], const int32_t uv[2], bool adc) {
    // p = T + R0 x + R1 y + R2 z; Q = proj / p.w; xyz = p.xyz Q + offset; STQ = (s, t, 1) / 4096 Q;
    // F = min(max(T.w + offset.w, fog_min), 255) (+ 3072 with ADC: bit 15 of the XYZF2 word)
    float p[4];
    for (int l = 0; l < 4; ++l) {
        float a = k.rows[3][l];
        a = a + k.rows[0][l] * static_cast<float>(pos[0]);
        a = a + k.rows[1][l] * static_cast<float>(pos[1]);
        p[l] = a + k.rows[2][l] * static_cast<float>(pos[2]);
    }
    const float q = k.proj / p[3];
    GsVertex v;
    v.s = static_cast<float>(uv[0]) * (1.0f / 4096.0f) * q;
    v.t = static_cast<float>(uv[1]) * (1.0f / 4096.0f) * q;
    v.q = 1.0f * q;
    for (int l = 0; l < 4; ++l) v.rgba[l] = rgba[l];
    auto fx = [](float f) { return ftoi4(f); };
    v.x = fx(p[0] * q + k.offset[0]);
    v.y = fx(p[1] * q + k.offset[1]);
    v.z = fx(p[2] * q + k.offset[2]);
    float fog = std::min(std::max(k.rows[3][3] + k.offset[3], k.fog_min), 255.0f);
    if (adc) fog = fog + 3072.0f;
    v.f = fx(fog);
    v.adc = adc;
    return v;
}

void kick_triangles(const Packet& p, const Vu1Consts& k, const std::vector<std::array<int32_t, 3>>& pos, const std::vector<std::array<int32_t, 4>>& rgba,
                    std::vector<GsTriangle>& out, bool clip) {
    // per slot: p = T + R0 x + R1 y + R2 z and its Q
    const size_t n = pos.size();
    std::vector<std::array<float, 4>> pp(n + 1);
    std::vector<float> q(n + 1, 0.0f);
    for (size_t s = 1; s <= n; ++s) {
        for (int l = 0; l < 4; ++l) {
            float a = k.rows[3][l];
            a = a + k.rows[0][l] * static_cast<float>(pos[s - 1][0]);
            a = a + k.rows[1][l] * static_cast<float>(pos[s - 1][1]);
            pp[s][static_cast<size_t>(l)] = a + k.rows[2][l] * static_cast<float>(pos[s - 1][2]);
        }
        q[s] = k.proj / pp[s][3];
    }
    auto fx = [](float f) { return ftoi4(f); };
    const float fog0 = std::min(std::max(k.rows[3][3] + k.offset[3], k.fog_min), 255.0f);
    GsVertex qv[2];
    uint8_t qs[2] = {};
    int m = 0, tex = -1;
    for (const KickOp& op : clip ? p.kicks_clip : p.kicks) {
        if (op.kind == KickKind::Restart) {
            m = 0;
            continue;
        }
        if (op.kind == KickKind::Texture) {
            tex = op.texture;
            continue;
        }
        auto qs_ok = [n](uint8_t s) { return s == 0xff || (s >= 1 && s <= n); };
        auto qv_of = [&](uint8_t s) { return s == 0xff ? k.q_start : q[s]; };
        if (op.pos == 0 || op.pos > n || !qs_ok(op.pos_q) || op.col == 0 || op.col > n || op.uv == 0 || op.uv > n || !qs_ok(op.uv_q)) continue;
        GsVertex v;
        float qq = qv_of(op.uv_q);
        if (op.uv_q2 != 0 && qs_ok(op.uv_q2)) qq = qq * qv_of(op.uv_q2);
        v.s = static_cast<float>(p.uv[op.uv - 1u][0]) * (1.0f / 4096.0f) * qq;
        v.t = static_cast<float>(p.uv[op.uv - 1u][1]) * (1.0f / 4096.0f) * qq;
        v.q = 1.0f * qq;
        for (int l = 0; l < 4; ++l) v.rgba[l] = rgba[op.col - 1u][static_cast<size_t>(l)];
        const float qp = qv_of(op.pos_q);
        v.x = fx(pp[op.pos][0] * qp + k.offset[0]);
        v.y = fx(pp[op.pos][1] * qp + k.offset[1]);
        v.z = fx(pp[op.pos][2] * qp + k.offset[2]);
        const float fog = clip ? std::min(std::max(pp[op.pos][3] + k.offset[3], k.fog_min), 255.0f) : fog0;
        v.f = fx(op.adc ? fog + 3072.0f : fog);
        v.adc = op.adc;
        if (m >= 2 && !op.adc) {
            GsTriangle t;
            t.v[0] = qv[0];
            t.v[1] = qv[1];
            t.v[2] = v;
            t.slot[0] = qs[0];
            t.slot[1] = qs[1];
            t.slot[2] = op.pos;
            t.texture = tex;
            out.push_back(t);
        }
        qv[0] = qv[1];
        qs[0] = qs[1];
        qv[1] = v;
        qs[1] = op.pos;
        m = std::min(m + 1, 2);
    }
}

}  // namespace rac1::mrp
