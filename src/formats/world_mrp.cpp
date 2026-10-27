// The MRP world asset: its file format and the CPU reference of the world's VU1 programs. See world_mrp.h and

#include <formats/world_mrp.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <type_traits>

namespace rac1::mrpw {

namespace {

// One function per type serves both directions (W writes, R reads).
struct W {
    std::vector<uint8_t> b;
    bool ok = true;
    template <typename T> void raw(T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        b.insert(b.end(), p, p + sizeof(T));
    }
    void size(size_t& n) {
        uint32_t v = static_cast<uint32_t>(n);
        raw(v);
    }
};
struct R {
    std::vector<uint8_t> b;
    size_t at = 0;
    bool ok = true;
    template <typename T> void raw(T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (at + sizeof(T) > b.size()) {
            ok = false;
            std::memset(&v, 0, sizeof(T));
            return;
        }
        std::memcpy(&v, b.data() + at, sizeof(T));
        at += sizeof(T);
    }
    void size(size_t& n) {
        uint32_t v = 0;
        raw(v);
        if (v > (1u << 26)) ok = false;
        n = ok ? v : 0;
    }
};

template <typename A> void io(A& a, Op& o);
template <typename A> void io(A& a, TexturePrim& t);
template <typename A> void io(A& a, ShrubPacket& p);
template <typename A> void io(A& a, ShrubClass& c);
template <typename A> void io(A& a, TiePacket& p);
template <typename A> void io(A& a, TieClass& c);
template <typename A> void io(A& a, TfragImage& t);
template <typename A> void io(A& a, TfragEntry& t);
template <typename A> void io(A& a, ShrubKickVertex& v);
template <typename A> void io(A& a, TieKickVertex& v);

// (a type with an io() goes field by field: no padding bytes in the file; the others raw)
template <typename A, typename T> void vec(A& a, std::vector<T>& v) {
    size_t n = v.size();
    a.size(n);
    if (!a.ok) return;
    v.resize(n);
    for (auto& x : v) {
        if constexpr (requires { io(a, x); }) io(a, x);
        else a.raw(x);
    }
}
template <typename A> void str(A& a, std::string& s) {
    std::vector<uint8_t> v(s.begin(), s.end());
    vec(a, v);
    s.assign(v.begin(), v.end());
}

template <typename A> void io(A& a, ShrubKickVertex& v) {
    a.raw(v.pos);
    a.raw(v.q);
    a.raw(v.uv);
    a.raw(v.colour);
}
template <typename A> void io(A& a, TieKickVertex& v) {
    a.raw(v.pos);
    a.raw(v.q);
    a.raw(v.uv);
    a.raw(v.colour);
    a.raw(v.fat);
    a.raw(v.colour2);
}
template <typename A> void io(A& a, Op& o) {
    a.raw(o.kind);
    a.raw(o.index);
    a.raw(o.tag);
}
template <typename A> void io(A& a, TexturePrim& t) {
    a.raw(t.ad);
    a.raw(t.n);
    a.raw(t.local_texture);
    a.raw(t.texture);
}
template <typename A> void io(A& a, ShrubPacket& p) {
    vec(a, p.vif);
    a.raw(p.header);
    vec(a, p.pos);
    vec(a, p.uv);
    vec(a, p.textures);
    vec(a, p.ops);
    vec(a, p.verts);
    a.raw(p.kick_ok);
    str(a, p.kick_note);
}
template <typename A> void io(A& a, ShrubClass& c) {
    a.raw(c.o_class);
    a.raw(c.index);
    vec(a, c.header);
    a.raw(c.texture_remap);
    vec(a, c.packets);
}
template <typename A> void io(A& a, TiePacket& p) {
    a.raw(p.lod);
    a.raw(p.index_in_lod);
    a.raw(p.entry);
    vec(a, p.data);
    vec(a, p.prims);
    a.raw(p.n_dinky);
    a.raw(p.n_fat);
    vec(a, p.dinky);
    vec(a, p.fat);
    vec(a, p.colour_index);
    vec(a, p.ops);
    vec(a, p.verts);
    a.raw(p.kick_ok);
    str(a, p.kick_note);
}
template <typename A> void io(A& a, TieClass& c) {
    a.raw(c.o_class);
    a.raw(c.index);
    vec(a, c.header);
    a.raw(c.texture_remap);
    a.raw(c.scale);
    vec(a, c.textures);
    vec(a, c.packets);
}
template <typename A> void io(A& a, TfragImage& t) {
    a.raw(t.colour_addr);
    a.raw(t.colour_count);
    a.raw(t.refs);
}
template <typename A> void io(A& a, TfragEntry& t) {
    a.raw(t.index);
    a.raw(t.header);
    vec(a, t.data);
    for (auto& l : t.lods) io(a, l);
    vec(a, t.textures);
}
template <typename A> void io(A& a, World& w) {
    uint32_t magic = kWorldMagic, version = kWorldVersion;
    a.raw(magic);
    a.raw(version);
    if (magic != kWorldMagic || version != kWorldVersion) {
        a.ok = false;
        return;
    }
    a.raw(w.level);
    vec(a, w.shrubs);
    vec(a, w.ties);
    vec(a, w.tfrags);
}

}  // namespace

bool write_world(const std::string& path, const World& w0) {
    W a;
    World w = w0;
    io(a, w);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(a.b.data()), static_cast<std::streamsize>(a.b.size()));
    return static_cast<bool>(f);
}

bool read_world(const std::string& path, World& w) {
    R a;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    a.b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    io(a, w);
    return a.ok;
}

// ---- the CPU reference ----

namespace {
// FTOI4: x 16 to an integer, saturated as the VU does
int32_t ftoi4(float f) {
    const float v = f * 16.0f;
    if (std::isnan(v)) return 0x7fffffff;
    if (!(v < 2147483648.0f)) return 0x7fffffff;
    if (!(v > -2147483648.0f)) return static_cast<int32_t>(0x80000000u);
    return static_cast<int32_t>(v);
}
uint32_t bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
}  // namespace

// orig: (host) the shrub A draw (micro 0x063) for one instance, as its kick program says (world_mrp.h)
void shrub_kick(const ShrubPacket& p, const ShrubInstance& in, const ShrubConsts& k, std::span<const std::array<uint32_t, 4>> tex_ad, Kicked& out) {
    out.q.clear();
    out.part.clear();
    out.clip.clear();
    out.hp.clear();
    // the instance's fog: clamp(T.w x scale + offset, floor, ceiling)
    float fog = in.rows[3][3] * k.fog_scale;
    fog = fog + k.fog_offset;
    fog = std::max(fog, k.fog_min);
    fog = std::min(fog, k.fog_max);
    const int32_t f = ftoi4(fog);
    // per input vertex: its transformed position (T + R0 x + R1 y + R2 z) and Q = 1 / w
    const size_t n = p.pos.size();
    std::vector<std::array<float, 4>> pp(n);
    std::vector<float> q(n);
    for (size_t i = 0; i < n; ++i) {
        const float x = static_cast<float>(p.pos[i][0]), y = static_cast<float>(p.pos[i][1]), z = static_cast<float>(p.pos[i][2]);
        for (int l = 0; l < 4; ++l) {
            float a = in.rows[3][l];
            a = a + in.rows[0][l] * x;
            a = a + in.rows[1][l] * y;
            pp[i][static_cast<size_t>(l)] = a + in.rows[2][l] * z;
        }
        q[i] = 1.0f / pp[i][3];
    }
    for (const Op& o : p.ops) {
        if (o.kind == OpKind::Tag || o.kind == OpKind::Other) {
            std::array<uint32_t, 4> w;
            std::memcpy(w.data(), o.tag, 16);
            out.q.push_back(w);
            out.part.push_back(0);
        } else if (o.kind == OpKind::Texture) {
            for (uint32_t r = 0; r < 4; ++r) {
                const size_t at = static_cast<size_t>(o.index) * 4 + r;
                out.q.push_back(at < tex_ad.size() ? tex_ad[at] : std::array<uint32_t, 4>{});
                out.part.push_back(1);
            }
        } else {
            const ShrubKickVertex& v = p.verts[o.index];
            const float qq = q[v.q];
            const auto& uv = p.uv[v.uv];
            const float s = static_cast<float>(uv[0]) * (1.0f / 4096.0f), t = static_cast<float>(uv[1]) * (1.0f / 4096.0f), h = static_cast<float>(uv[2]) * (1.0f / 4096.0f);
            out.q.push_back({bits(s * qq), bits(t * qq), bits(h * qq), 0});
            out.part.push_back(2);
            int32_t mc[4] = {};
            if (v.colour >= kTwiceBase && in.vu_mem != nullptr) std::memcpy(mc, in.vu_mem + (((2 * (in.block + 5) + (v.colour - kTwiceBase)) & 0x3ff) << 4), 16);
            const int32_t* c = v.colour >= kTwiceBase ? mc : in.colour[v.colour < 24 ? v.colour : 0];
            out.q.push_back({static_cast<uint32_t>(c[0]), static_cast<uint32_t>(c[1]), static_cast<uint32_t>(c[2]), static_cast<uint32_t>(in.alpha)});
            out.part.push_back(3);
            const auto& P = pp[v.pos];
            if (!k.clip) {
                out.q.push_back({static_cast<uint32_t>(ftoi4(P[0] * qq)), static_cast<uint32_t>(ftoi4(P[1] * qq)), static_cast<uint32_t>(ftoi4(P[2] * qq)), static_cast<uint32_t>(f)});
            } else {
                // (shrub B: STQ was (s, t) x (h Q), h Q; the screen ((p x h Q) x scale) + offset)
                const float hq = h * qq;
                out.q[out.q.size() - 2] = {bits(s * hq), bits(t * hq), bits(hq), 0};
                uint32_t xyz[3];
                for (int l = 0; l < 3; ++l) xyz[l] = static_cast<uint32_t>(ftoi4((P[static_cast<size_t>(l)] * hq) * k.scale[l] + k.offset[l]));
                out.q.push_back({xyz[0], xyz[1], xyz[2], static_cast<uint32_t>(f)});
            }
            out.part.push_back(4);
            out.clip.resize(out.q.size(), 0);
            const float aw = std::fabs(P[3]);
            out.clip.back() = (std::fabs(P[0]) > aw || std::fabs(P[1]) > aw || std::fabs(P[2]) > aw) ? 1 : 0;
            if (out.want_hp) {
                out.hp.resize(out.q.size(), std::array<float, 4>{});
                out.hp.back() = P;
            }
        }
    }
    if (out.want_hp) out.hp.resize(out.q.size(), std::array<float, 4>{});
}

namespace {
float f32(uint32_t u) {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
}  // namespace

// orig: (host) the tie A draw (micro 0x000) for one instance, as its kick program says (world_mrp.h)
void tie_kick(const TiePacket& p, const TieInstance& in, std::span<const std::array<uint32_t, 4>> tex_ad, Kicked& out) {
    out.q.clear();
    out.part.clear();
    out.clip.clear();
    out.hp.clear();
    const uint32_t nd = p.n_dinky;
    const int32_t fog = ftoi4(in.fog[3]);
    // a vertex's position (fat: delta x morph + base) through the rows
    auto transform = [&](uint32_t k, float pp[4]) {
        float P[3];
        if (k < nd) {
            for (int l = 0; l < 3; ++l) P[l] = static_cast<float>(p.dinky[k][static_cast<size_t>(l)]);
        } else {
            const auto& f = p.fat[k - nd];
            for (int l = 0; l < 3; ++l) {
                const float d = static_cast<float>(f[static_cast<size_t>(l)]) * in.morph[0];
                P[l] = d + static_cast<float>(f[static_cast<size_t>(4 + l)]);
            }
        }
        for (int l = 0; l < 4; ++l) {
            float a = in.rows[3][l];
            a = a + in.rows[0][l] * P[0];
            a = a + in.rows[1][l] * P[1];
            pp[l] = a + in.rows[2][l] * P[2];
        }
    };
    auto uv = [&](uint32_t k, int l) {
        const int16_t v = k < nd ? p.dinky[k][static_cast<size_t>(4 + l)] : p.fat[k - nd][static_cast<size_t>(8 + l)];
        return static_cast<float>(v) * (1.0f / 4096.0f);
    };
    static thread_local std::vector<std::array<float, 4>> tp;  // (static: the Wii U build defines thread_local away)
    static thread_local std::vector<uint8_t> done;
    const size_t nv = static_cast<size_t>(nd) + p.n_fat;
    tp.resize(nv);
    done.assign(nv, 0);
    out.q.reserve(p.ops.size() * 3);
    out.part.reserve(p.ops.size() * 3);
    for (const Op& o : p.ops) {
        if (o.kind == OpKind::Tag || o.kind == OpKind::Other) {
            std::array<uint32_t, 4> w;
            std::memcpy(w.data(), o.tag, 16);
            out.q.push_back(w);
            out.part.push_back(o.kind == OpKind::Other ? 1 : 0);
        } else if (o.kind == OpKind::Texture) {
            for (uint32_t r = 0; r < 5; ++r) {
                const size_t at = static_cast<size_t>(o.index) * 5 + r;
                out.q.push_back(at < tex_ad.size() ? tex_ad[at] : std::array<uint32_t, 4>{});
                out.part.push_back(1);
            }
        } else {
            const TieKickVertex& v = p.verts[o.index];
            // (each input vertex transformed once: the same operations as per kicked vertex)
            if (done[v.q] == 0) {
                transform(v.q, tp[v.q].data());
                done[v.q] = 1;
            }
            if (done[v.pos] == 0) {
                transform(v.pos, tp[v.pos].data());
                done[v.pos] = 1;
            }
            const float* pq = tp[v.q].data();
            const float* pp = tp[v.pos].data();
            const float q = 1.0f / pq[3];
            out.q.push_back({bits(uv(v.uv, 0) * q), bits(uv(v.uv, 1) * q), bits(uv(v.uv, 2) * q), 0});
            out.part.push_back(2);
            std::array<uint32_t, 4> c;
            if (v.fat == 0) {
                for (int l = 0; l < 4; ++l) c[static_cast<size_t>(l)] = in.colour[v.colour & 63][l];
            } else {
                for (int l = 0; l < 4; ++l) {
                    const float avg = f32(in.colour[v.colour2[0] & 63][l]) * 0.5f + f32(in.colour[v.colour2[1] & 63][l]) * 0.5f;
                    c[static_cast<size_t>(l)] = bits(avg * in.morph[3] + f32(in.colour[v.colour & 63][l]) * in.morph[2]);
                }
            }
            out.q.push_back(c);
            out.part.push_back(3);
            if (!in.clip) {
                out.q.push_back({static_cast<uint32_t>(ftoi4(pp[0] * q)), static_cast<uint32_t>(ftoi4(pp[1] * q)), static_cast<uint32_t>(ftoi4(pp[2] * q)), static_cast<uint32_t>(fog)});
            } else {
                // (tie B: the screen ((p x scale) x (q Q)) + offset, F the fog)
                const float qq = uv(v.uv, 2) * q;
                uint32_t xyz[3];
                for (int l = 0; l < 3; ++l) xyz[l] = static_cast<uint32_t>(ftoi4((pp[l] * in.scale[l]) * qq + in.offset[l]));
                out.q.push_back({xyz[0], xyz[1], xyz[2], static_cast<uint32_t>(ftoi4(in.fog[3]))});
            }
            out.part.push_back(4);
            out.clip.resize(out.q.size(), 0);
            const float aw = std::fabs(pp[3]);
            out.clip.back() = (std::fabs(pp[0]) > aw || std::fabs(pp[1]) > aw || std::fabs(pp[2]) > aw) ? 1 : 0;
            if (out.want_hp) {
                out.hp.resize(out.q.size(), std::array<float, 4>{});
                out.hp.back() = {pp[0], pp[1], pp[2], pp[3]};
            }
        }
    }
    if (out.want_hp) out.hp.resize(out.q.size(), std::array<float, 4>{});
}

TfragCounts g_tfrag_counts;

namespace {

// The VU1 data memory as the tfrag reference sees it (a copy it modifies as the program does).
struct VuMem {
    std::vector<std::array<uint32_t, 4>> q = std::vector<std::array<uint32_t, 4>>(0x400);
    std::array<uint32_t, 4>& at(uint32_t a) { return q[a & 0x3ff]; }
    float f(uint32_t a, int l) { return f32(at(a)[static_cast<size_t>(l)]); }
    void setf(uint32_t a, int l, float v) { at(a)[static_cast<size_t>(l)] = bits(v); }
    int16_t i16(uint32_t a, int l) { return static_cast<int16_t>(at(a)[static_cast<size_t>(l)] & 0xffff); }  // (ILW / ILWR: the low 16 bits)
    uint16_t u16(uint32_t a, int l) { return static_cast<uint16_t>(at(a)[static_cast<size_t>(l)] & 0xffff); }
};
struct V4 {
    float v[4] = {};
};
V4 ld(VuMem& m, uint32_t a) {
    V4 r;
    for (int l = 0; l < 4; ++l) r.v[l] = m.f(a, l);
    return r;
}
V4 itof0(VuMem& m, uint32_t a) {
    V4 r;
    for (int l = 0; l < 4; ++l) r.v[l] = static_cast<float>(static_cast<int32_t>(m.at(a)[static_cast<size_t>(l)]));
    return r;
}
void st(VuMem& m, uint32_t a, const V4& v) {
    for (int l = 0; l < 4; ++l) m.setf(a, l, v.v[l]);
}
// p = T + R0 x + R1 y + R2 z (the FMAC chain's order), the matrix rows at r0..r0 + 3 (R0, R1, R2, T)
V4 xform(VuMem& m, uint32_t r0, const V4& p) {
    V4 o;
    for (int l = 0; l < 4; ++l) {
        float a = m.f(r0 + 3, l);
        a = a + m.f(r0, l) * p.v[0];
        a = a + m.f(r0 + 1, l) * p.v[1];
        o.v[l] = a + m.f(r0 + 2, l) * p.v[2];
    }
    return o;
}
int32_t ftoi0(float f) {
    if (std::isnan(f)) return 0x7fffffff;
    if (!(f < 2147483648.0f)) return 0x7fffffff;
    if (!(f > -2147483648.0f)) return static_cast<int32_t>(0x80000000u);
    return static_cast<int32_t>(f);
}

// the header (5 qwords at the buffer base, unpacked V4_16 unsigned): its fields by (qword, lane)
uint32_t hdr(VuMem& m, uint32_t base, int q, int l) { return m.u16(base + static_cast<uint32_t>(q), l); }

// T (micro 0x04f / 0x052): `count` records from `rec` (buffer-relative): position and colour converted, the position through
// the view matrix
void tf_transform(VuMem& m, uint32_t base, uint32_t rec, uint32_t count) {
    for (uint32_t k = 0; k < count; ++k) {
        const uint32_t a = base + rec + 2 * k;
        const V4 p = xform(m, base + 5, itof0(m, a));
        const V4 c = itof0(m, a + 1);
        st(m, a, p);
        st(m, a + 1, c);
    }
}

// M / MS (micro 0x093 / 0x08a / 0x156 / 0x160): `count` vertices of a vertex-info list `vi`, each morphed toward its two
// parents (the record of the vertex info its parent index names; the record its own z names) by the band at `band`
// (0x29a / 0x29b: k, offsets 0x29c / 0x29d); with `subst`, a vertex whose two parents are past the band's threshold (w)
// takes its parent's vertex info instead (its record left as it was)
void tf_morph(VuMem& m, uint32_t base, uint32_t vi, uint32_t pi, uint32_t count, uint32_t band, bool subst) {
    const float kx = m.f(band, 0), ky = m.f(band, 1), cx = m.f(band + 2, 0), cy = m.f(band + 2, 1), thr = m.f(band, 3);
    const float mx = m.f(0x291, 0), my = m.f(0x291, 1);
    for (uint32_t k = 0; k < count; ++k) {
        const uint32_t via = base + vi + k;
        const uint32_t rec = base + m.u16(via, 3);
        const uint32_t pvi = base + static_cast<uint32_t>(m.at(base + pi + k / 4)[k % 4] & 0xffff);
        const uint32_t r1 = base + m.u16(pvi, 3), r2 = base + m.u16(via, 2);
        const V4 p = xform(m, base + 5, itof0(m, rec));
        const V4 c = itof0(m, rec + 1);
        const V4 P1 = ld(m, r1), C1 = ld(m, r1 + 1), P2 = ld(m, r2), C2 = ld(m, r2 + 1);
        float fx = cx * 1.0f + kx * p.v[3], fy = cy * 1.0f + ky * p.v[3];
        fx = std::max(std::min(fx, mx), 0.0f);
        fy = std::max(std::min(fy, my), 0.0f);
        V4 S, SC, po, co;
        for (int l = 0; l < 4; ++l) {
            S.v[l] = P1.v[l] + P2.v[l];
            SC.v[l] = C1.v[l] + C2.v[l];
            po.v[l] = S.v[l] * fx + p.v[l] * fy;
            co.v[l] = SC.v[l] * fx + c.v[l] * fy;
        }
        ++g_tfrag_counts.morphed;
        if (subst && (P1.v[3] - thr) < 0.0f && (P2.v[3] - thr) < 0.0f) {
            m.at(via) = m.at(pvi);
            ++g_tfrag_counts.substituted;
            continue;
        }
        st(m, rec, po);
        st(m, rec + 1, co);
    }
}

// F (micro 0x278 / 0x27d): `count` entries of the list `list`: an entry whose record (its z) and the record of the vertex
// info the second index list names are both past the threshold takes that vertex info
void tf_fix(VuMem& m, uint32_t base, uint32_t list, uint32_t idx2, uint32_t count, float thr) {
    for (uint32_t j = 0; j < count; ++j) {
        const uint32_t la = base + list + j;
        const uint32_t vi = base + static_cast<uint32_t>(m.at(base + idx2 + j / 4)[j % 4] & 0xffff);
        const uint32_t ra = base + m.u16(la, 2), rb = base + m.u16(vi, 3);
        if ((m.f(ra, 3) - thr) < 0.0f && (m.f(rb, 3) - thr) < 0.0f) {
            m.at(la) = m.at(vi);
            ++g_tfrag_counts.fixed;
        }
    }
}

// The strips' GIF packets (micro 0x45d / 0x382 / 0x51d / 0x652 and their strip change 0x332 / 0x5fd). `form`: 0 the
// morphed records (0x45d), 1 the morphed records with the guard band (0x382), 2 the records transformed here (0x51d),
// 3 the records transformed here with the guard band (0x652).
void tf_emit(VuMem& m, uint32_t base, int form, std::vector<Kicked>& out, bool want_hp) {
    // (form 4: tfrag B, see bform)
    const uint32_t tex = base + hdr(m, base, 4, 3), strips = base + hdr(m, base, 4, 2), index = base + hdr(m, base, 3, 1);
    const V4 c0 = ld(m, 0x290), c4 = ld(m, 0x298), c5 = ld(m, 0x295), c6 = ld(m, 0x296);
    // the S / T offset (the vertex info's s, t are 2048 + value floats): the band constant's z (0x45d, 0x51d, 0x652) or the
    // first texture prim's second qword's w (0x382)
    const bool bform = form == 4;  // (tfrag B: 0x652's math, the first prim's offset, the clip test flags instead of ADC)
    if (bform) form = 3;
    const float stoff = (form == 1 || bform) ? m.f(tex + 1, 3) : m.f(0x291, 2);
    Kicked cur;
    cur.want_hp = want_hp;
    uint8_t cur_clip = 0;
    std::array<float, 4> cur_hp{};
    auto put = [&](const std::array<uint32_t, 4>& q, uint8_t part) {
        cur.q.push_back(q);
        cur.part.push_back(part);
        cur.clip.push_back(part == 4 ? cur_clip : 0);
        if (want_hp) cur.hp.push_back(part == 4 ? cur_hp : std::array<float, 4>{});
    };
    auto texture = [&](uint32_t t) {
        put(m.at(0x294), 0);
        for (uint32_t r = 0; r < 5; ++r) put(m.at(t + r), 1);
    };
    size_t tag_at = 0;
    auto strip_tag = [&](uint32_t n) {
        tag_at = cur.q.size();
        std::array<uint32_t, 4> t = m.at(0x292);
        t[0] = (t[0] & 0xffff0000u) | (n & 0xffff);
        put(t, 0);
    };
    uint32_t si = strips;
    int32_t n = m.i16(si, 0) + 0x80;  // (the first strip always switches the texture)
    texture(tex + static_cast<uint32_t>(m.i16(si, 2)));
    strip_tag(static_cast<uint32_t>(n));
    ++si;
    uint32_t ix = 0;  // the index byte
    uint32_t clip_hist = 0;  // the last clip tests (6 bits per vertex)
    bool strip_first = false;  // the next vertex follows a strip change
    for (int guard = 0; guard < 100000; ++guard) {
        for (int32_t v = 0; v < n; ++v, ++ix) {
            const uint32_t via = base + static_cast<uint32_t>(m.at(index + ix / 4)[ix % 4] & 0xffff);
            const uint32_t rec = base + m.u16(via, 3);
            V4 P, C;
            if (form <= 1) {
                P = ld(m, rec);
                C = ld(m, rec + 1);
            } else {
                P = xform(m, base + 5, itof0(m, rec));
            }
            V4 stq;
            stq.v[0] = m.f(via, 0) - stoff;
            stq.v[1] = m.f(via, 1) - stoff;
            stq.v[2] = (1.0f + stoff) - stoff;
            bool adc = false;
            if (form == 1 || form == 3) {
                // the guard band: clipw of P x the scale (6) against its w; ADC when this or one of the two vertices before is outside
                V4 g;
                for (int l = 0; l < 4; ++l) g.v[l] = P.v[l] * c6.v[l];
                const float w = std::fabs(g.v[3]);
                uint32_t f6 = 0;
                if (g.v[0] > w) f6 |= 1;
                if (g.v[0] < -w) f6 |= 2;
                if (g.v[1] > w) f6 |= 4;
                if (g.v[1] < -w) f6 |= 8;
                if (g.v[2] > w) f6 |= 16;
                if (g.v[2] < -w) f6 |= 32;
                // (the first vertex after a strip change: the program ran its CLIPW in the delay slot of the branch to the strip
                // change, whose FCSET then cleared it -- its flags never count)
                clip_hist = ((clip_hist << 6) | (strip_first ? 0u : f6)) & 0xffffff;
                strip_first = false;
                adc = (clip_hist & 0x3ffff) != 0;
                cur_clip = f6 != 0 ? 1 : 0;
                if (bform) adc = false;
            }
            cur_hp = {P.v[0], P.v[1], P.v[2], P.v[3]};
            const float q = c0.v[0] / P.v[3];
            for (int l = 0; l < 3; ++l) {
                P.v[l] = P.v[l] * q;
                stq.v[l] = stq.v[l] * q;
            }
            for (int l = 0; l < 4; ++l) P.v[l] = P.v[l] + c5.v[l];
            if (form == 3) {
                P.v[3] = std::min(P.v[3], c0.v[2]);
                P.v[3] = std::max(P.v[3], c0.v[1]);
            } else {
                P.v[3] = std::max(P.v[3], c0.v[1]);
                P.v[3] = std::min(P.v[3], c0.v[2]);
            }
            if (adc) {
                P.v[3] = P.v[3] + c0.v[3];
                ++g_tfrag_counts.adc;
            }
            ++g_tfrag_counts.vertices;
            put({bits(stq.v[0]), bits(stq.v[1]), bits(stq.v[2]), 0}, 2);
            if (form <= 1) {
                std::array<uint32_t, 4> rgba;
                for (int l = 0; l < 4; ++l) rgba[static_cast<size_t>(l)] = static_cast<uint32_t>(ftoi0(C.v[l] * c4.v[l]));
                put(rgba, 3);
            } else {
                put(m.at(rec + 1), 3);
            }
            put({static_cast<uint32_t>(ftoi4(P.v[0])), static_cast<uint32_t>(ftoi4(P.v[1])), static_cast<uint32_t>(ftoi4(P.v[2])), static_cast<uint32_t>(ftoi4(P.v[3]))},
                4);
        }
        // the strip change (0x332): the next strip entry (count, end of packet, texture)
        clip_hist = 0;
        strip_first = true;
        const int32_t nx = m.i16(si, 0);
        if (nx > 0) {
            ++si;
            n = nx;
            strip_tag(static_cast<uint32_t>(n));
            continue;
        }
        const int32_t eop = m.i16(si, 1), tz = m.i16(si, 2);
        ++si;
        cur.q[tag_at][0] |= 0x8000u;  // (EOP on the last strip tag)
        if (nx == 0) {
            out.push_back(std::move(cur));
            return;
        }
        n = nx + 0x80;
        if (eop < 0) {
            out.push_back(std::move(cur));
            cur = Kicked{};
            cur.want_hp = want_hp;
            if (tz >= 0) texture(tex + static_cast<uint32_t>(tz));
        } else {
            cur.q[tag_at][0] &= ~0x8000u;
            texture(tex + static_cast<uint32_t>(tz));
        }
        strip_tag(static_cast<uint32_t>(n));
    }
}

}  // namespace

// orig: (host) the tfrag B program's start 0x002, as far as its unclipped triangles go (world_mrp.h)
bool tfrag_b_kick(const uint8_t* vu_mem, uint32_t base, std::vector<Kicked>& out, bool want_hp) {
    out.clear();
    VuMem m;
    for (uint32_t a = 0; a < 0x400; ++a) std::memcpy(m.q[a].data(), vu_mem + 16 * a, 16);
    tf_emit(m, base, 4, out, want_hp);
    return true;
}

// orig: (host) the tfrag A program for one start, re-implemented (world_mrp.h)
bool tfrag_kick(const uint8_t* vu_mem, uint32_t base, uint32_t start, std::vector<Kicked>& out, bool want_hp) {
    out.clear();
    VuMem m;
    for (uint32_t a = 0; a < 0x400; ++a) std::memcpy(m.q[a].data(), vu_mem + 16 * a, 16);
    auto H = [&](int q, int l) { return hdr(m, base, q, l); };
    const uint32_t common = H(0, 0), lod01 = H(0, 2), lod0 = H(1, 0), recs = H(1, 2);
    switch (start) {
    case 0x006: case 0x00c: case 0x014:  // far: the strips from the raw records, the guard band
        tf_emit(m, base, 3, out, want_hp);
        return true;
    case 0x004:  // the raw records, no guard band
        tf_emit(m, base, 2, out, want_hp);
        return true;
    case 0x008:  // common; lod 0 morphed with substitutions (band A); the list fix-up (band A)
        tf_transform(m, base, recs, common);
        tf_morph(m, base, H(2, 1), H(3, 2), lod01, 0x29a, true);
        tf_fix(m, base, H(2, 2), H(3, 3), H(0, 3), m.f(0x29a, 3));
        tf_emit(m, base, 0, out, want_hp);
        return true;
    case 0x00a:  // common; lod 01 morphed (band A)
        tf_transform(m, base, recs, common);
        tf_morph(m, base, H(2, 1), H(3, 2), lod01, 0x29a, false);
        tf_emit(m, base, 0, out, want_hp);
        return true;
    case 0x00e:  // common; lod 01 morphed (band A); lod 0 morphed with substitutions (band B); the list fix-up (band B)
        tf_transform(m, base, recs, common);
        tf_morph(m, base, H(2, 1), H(3, 2), lod01, 0x29a, false);
        tf_morph(m, base, H(2, 3), H(4, 0), lod0, 0x29b, true);
        tf_fix(m, base, H(3, 0), H(4, 1), H(1, 1), m.f(0x29b, 3));
        tf_emit(m, base, 0, out, want_hp);
        return true;
    case 0x010:  // common + lod 01; lod 0 morphed (band B)
        tf_transform(m, base, recs, common + lod01);
        tf_morph(m, base, H(2, 3), H(4, 0), lod0, 0x29b, false);
        tf_emit(m, base, 0, out, want_hp);
        return true;
    case 0x012:  // the same, the strips with the guard band
        tf_transform(m, base, recs, common + lod01);
        tf_morph(m, base, H(2, 3), H(4, 0), lod0, 0x29b, false);
        tf_emit(m, base, 1, out, want_hp);
        return true;
    default:
        return false;
    }
}

}  // namespace rac1::mrpw
