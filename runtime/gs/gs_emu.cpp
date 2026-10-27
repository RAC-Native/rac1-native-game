// See gs_emu.h. The local memory (its layouts, the uploads, the texture decodes) is gs_memory.cpp's.
#include <runtime/vu/vu1.h>
#include <runtime/gs/gs_emu.h>

#include <runtime/kernel/ee_memory.h>
#include <profiler/host_profiler.h>

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <iterator>
#include <mutex>
#include <thread>
#include <cmath>
#include <cstring>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace rac1::gs {

namespace {
uint64_t r64(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}
uint32_t r32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
float f32(uint32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
uint32_t bits(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }
}  // namespace

Gs::Gs() : mem_(local_memory()) {}

Gs& gs_global() {
    static Gs gs;
    return gs;
}

void Gs::vertex_kick(const Vertex& vin, bool draw) {
    if (vu7_drop_ && vu_kick_ && !vu7_batches_.empty()) {  // (host: a kept batch's strip as the program kicked it, its ADC flags)
        const uint32_t t = bits(regs_[0x00], 0, 3);
        if (t == 3 || t == 4) vu7_batches_.back().kicked.push_back(draw ? 1 : 0);
    }
    const uint64_t prim = bits(regs_[0x1a], 0, 1) ? regs_[0x00] : (regs_[0x1b] & ~7ull) | (regs_[0x00] & 7);
    const int ctx = bits(prim, 9, 1);
    const uint64_t off = regs_[0x18 + ctx];
    Vertex v = vin;
    v.x = (v.x - static_cast<float>(bits(off, 0, 16))) / 16.0f;
    v.y = (v.y - static_cast<float>(bits(off, 32, 16))) / 16.0f;
    queue_.push_back(v);
    const int type = bits(regs_[0x00], 0, 3);
    int need = 0, assembled = 0;
    switch (type) {
    case 0: need = 1; assembled = 0; break;
    case 1: case 2: need = 2; assembled = 1; break;
    case 3: case 4: case 5: need = 3; assembled = 3; break;
    case 6: need = 2; assembled = 6; break;
    default: queue_.clear(); return;
    }
    if (static_cast<int>(queue_.size()) < need) return;
    if (draw && type != 0) {
        if (scr_prim_ >= 0) screen_picture();  // (the frame's picture put back before this primitive)
        Primitive p;
        p.type = assembled;
        DrawState& s = p.st;
        s.prim = prim;
        s.tex0 = regs_[0x06 + ctx];
        s.clamp = regs_[0x08 + ctx];
        s.tex1 = regs_[0x14 + ctx];
        s.miptbp1 = regs_[0x34 + ctx];
        s.alpha = regs_[0x42 + ctx];
        s.test = regs_[0x47 + ctx];
        s.scissor = regs_[0x40 + ctx];
        s.fba = regs_[0x4a + ctx];
        s.frame = regs_[0x4c + ctx];
        s.zbuf = regs_[0x4e + ctx];
        s.texa = regs_[0x3b];
        s.colclamp = regs_[0x46];
        s.pabe = regs_[0x49];
        s.fogcol = regs_[0x3d];
        s.dthe = regs_[0x45];
        s.xyoffset = off;
        p.v.assign(queue_.end() - need, queue_.end());
        if (bits(prim, 4, 1) != 0) {
            // mipmapping (TEX1 MXL > 0, MMIN *_MIPMAP_*): the GS picks the level per pixel from
            // its LOD (LCM 0: log2(1 / |Q|) << L + K, Q interpolated; LCM 1: K), rounded, clamped
            // to MXL; level n's base / width from MIPTBP1 (levels 1..3), its size halved n times.
            // The levels the primitive's vertices span are read at the kick; the draw picks per pixel.
            const uint32_t mxl = std::min(bits(s.tex1, 2, 3), 3u);
            if (mxl != 0 && !bits(prim, 8, 1) && bits(s.tex1, 6, 3) >= 2) {
                const float k = static_cast<float>(static_cast<int32_t>(bits(s.tex1, 32, 12) << 20) >> 20) / 16.0f;
                auto level_of = [&](float q) {
                    if (std::isnan(q)) return 0;
                    const float lod = bits(s.tex1, 0, 1) != 0 ? k : std::log2(1.0f / std::max(std::fabs(q), 1e-20f)) * static_cast<float>(1u << bits(s.tex1, 19, 2)) + k;
                    if (std::isnan(lod) || lod <= 0.0f) return 0;
                    return std::min(static_cast<int>(std::floor(lod + 0.5f)), static_cast<int>(mxl));
                };
                int lo = 3, hi = 0;
                for (auto it = queue_.end() - need; it != queue_.end(); ++it) {
                    lo = std::min(lo, level_of(it->q));
                    hi = std::max(hi, level_of(it->q));
                }
                lo = std::clamp(lo, 0, static_cast<int>(mxl));
                hi = std::clamp(hi, lo, static_cast<int>(mxl));
                auto level_tex0 = [&](int level) -> uint64_t {
                    if (level == 0) return s.tex0;
                    const uint32_t lv = static_cast<uint32_t>(level - 1);
                    const uint64_t tbp = bits(s.miptbp1, static_cast<int>(20 * lv), 14), tbw = bits(s.miptbp1, static_cast<int>(20 * lv + 14), 6);
                    const uint64_t tw = bits(s.tex0, 26, 4), th = bits(s.tex0, 30, 4);
                    const uint64_t ntw = tw > lv + 1 ? tw - lv - 1 : 0, nth = th > lv + 1 ? th - lv - 1 : 0;
                    return (s.tex0 & ~((0x3fffull) | (0x3full << 14) | (0xfull << 26) | (0xfull << 30))) | tbp | (tbw << 14) | (ntw << 26) | (nth << 30);
                };
                p.mip_lo = lo;
                p.mip_n = hi - lo;
                for (int l = lo + 1; l <= hi; ++l) {
                    int idx = l - lo - 1;
                    if (idx >= 0 && idx < 3) {
                        p.mip_tex0[idx] = level_tex0(l);
                        p.mip_tex[idx] = mem_.texture_at_kick(p.mip_tex0[idx], s.texa);
                    }
                }
                s.tex0 = level_tex0(lo);
            }
            p.tex = mem_.texture_at_kick(s.tex0, s.texa);
        }
        if (!bits(prim, 3, 1))  // flat: the last vertex's colour
            for (Vertex& w : p.v) { w.r = p.v.back().r; w.g = p.v.back().g; w.b = p.v.back().b; w.a = p.v.back().a; }
        p.vu = vu_kick_;
        p.vu_start = vu_kick_ ? vu1::g_last_start : 0;
        p.moby = moby_kick_;
        // a substituted range: both kept, marked (the drawing takes the projected 3D from the substitute, the 2D from the
        // real range: gs_draw.cpp)
        p.sub = sub_wide_ ? 1 : sub_drop_ ? 2 : 0;
        p.wide = sub_wide_;
        if (sub_drop_) ++sub_dropped_;
        p.fx = fx_now_;
        p.fx_id = fx_id_;
        p.layer = layer_now_;
        p.hud = hud_now_;
        if (vu_kick_ && vu7_drop_) {  // (a batch the MRP draws itself: set_chain_vu7)
            p.fx = vu7_kind_;
            p.fx_id = kVu7Kicked;
        }
        prims_.push_back(std::move(p));
    }
    switch (type) {
    case 2: queue_.erase(queue_.begin(), queue_.end() - 1); break;
    case 4: queue_.erase(queue_.begin()); break;
    case 5: queue_.erase(queue_.begin() + 1); break;
    default: queue_.clear(); break;
    }
}

void Gs::write_reg(uint32_t reg, uint64_t v) {
    reg &= 0x7f;
    regs_[reg] = v;
    switch (reg) {
    case 0x00: queue_.clear(); break;  // PRIM
    case 0x01:
        cur_.r = bits(v, 0, 8); cur_.g = bits(v, 8, 8); cur_.b = bits(v, 16, 8); cur_.a = bits(v, 24, 8);
        cur_.q = f32(bits(v, 32, 32));
        break;
    case 0x02: cur_.s = f32(bits(v, 0, 32)); cur_.t = f32(bits(v, 32, 32)); break;
    case 0x03: cur_.u = bits(v, 0, 14) / 16.0f; cur_.v = bits(v, 16, 14) / 16.0f; break;
    case 0x0a: cur_.fog = bits(v, 56, 8); break;
    case 0x04: case 0x0c: {  // XYZF2 / XYZF3
        Vertex k = cur_;
        k.x = static_cast<float>(bits(v, 0, 16)); k.y = static_cast<float>(bits(v, 16, 16)); k.z = bits(v, 32, 24); k.fog = bits(v, 56, 8);
        vertex_kick(k, reg == 0x04);
        break;
    }
    case 0x05: case 0x0d: {  // XYZ2 / XYZ3
        Vertex k = cur_;
        k.x = static_cast<float>(bits(v, 0, 16)); k.y = static_cast<float>(bits(v, 16, 16)); k.z = bits(v, 32, 32);
        vertex_kick(k, reg == 0x05);
        break;
    }
    case 0x53:  // TRXDIR: host -> local
        if (bits(v, 0, 2) == 0) {
            mem_.transfer_start();
            if (scr_bw_ != 0) screen_transfer(bits(regs_[0x50], 32, 14));
        }
        else mem_.transfer_stop();
        break;
    default: break;
    }
}

void Gs::packed(const uint8_t* q, uint32_t d) {
    const uint32_t w0 = r32(q), w1 = r32(q + 4), w2 = r32(q + 8), w3 = r32(q + 12);
    switch (d) {
    case 0x0: write_reg(0x00, w0 & 0x7ff); break;
    case 0x1:
        cur_.r = w0 & 0xff; cur_.g = w1 & 0xff; cur_.b = w2 & 0xff; cur_.a = w3 & 0xff;
        regs_[0x01] = (w0 & 0xff) | ((w1 & 0xff) << 8) | ((w2 & 0xff) << 16) | (static_cast<uint64_t>(w3 & 0xff) << 24);
        break;
    case 0x2: cur_.s = f32(w0); cur_.t = f32(w1); cur_.q = f32(w2); break;
    case 0x3: cur_.u = (w0 & 0x3fff) / 16.0f; cur_.v = (w1 & 0x3fff) / 16.0f; break;
    case 0x4: case 0xc: {
        Vertex k = cur_;
        k.x = static_cast<float>(w0 & 0xffff); k.y = static_cast<float>(w1 & 0xffff); k.z = (w2 >> 4) & 0xffffff; k.fog = (w3 >> 4) & 0xff;
        vertex_kick(k, d == 0x4 && !((w3 >> 15) & 1));
        break;
    }
    case 0x5: case 0xd: {
        Vertex k = cur_;
        k.x = static_cast<float>(w0 & 0xffff); k.y = static_cast<float>(w1 & 0xffff); k.z = w2;
        vertex_kick(k, d == 0x5 && !((w3 >> 15) & 1));
        break;
    }
    case 0xa: cur_.fog = (w3 >> 4) & 0xff; break;
    case 0xe: write_reg(q[8], r64(q)); break;
    case 0xf: break;
    default: write_reg(d, r64(q)); break;  // TEX0 / CLAMP: the register's own layout
    }
}

// GIF data, a quadword at a time: a tag and its data may arrive in separate transfers (an
// IMAGE tag in a CNT, its pixels in the next REF), so the tag in progress is kept.
void Gs::gif(const uint8_t* p, uint32_t qwc) {
    for (uint32_t i = 0; i < qwc; ++i) {
        const uint8_t* q = p + i * 16;
        if (tag_left_ == 0) {
            const uint64_t lo = r64(q), hi = r64(q + 8);
            const uint32_t nloop = bits(lo, 0, 15);
            tag_flg_ = bits(lo, 58, 2);
            tag_nreg_ = bits(lo, 60, 4);
            if (tag_nreg_ == 0) tag_nreg_ = 16;
            tag_regs_ = hi;
            tag_reg_ = 0;
            if (tag_flg_ == 0 && bits(lo, 46, 1)) write_reg(0x00, bits(lo, 47, 11));
            // the data's size: PACKED nloop x nreg quadwords, REGLIST nloop x nreg double words, IMAGE nloop quadwords
            tag_left_ = tag_flg_ == 0 ? nloop * tag_nreg_ : tag_flg_ == 1 ? nloop * tag_nreg_ : nloop;
            continue;
        }
        if (tag_flg_ == 0) {
            packed(q, bits(tag_regs_, static_cast<int>(4 * tag_reg_), 4));
            tag_reg_ = (tag_reg_ + 1) % tag_nreg_;
            --tag_left_;
        } else if (tag_flg_ == 1) {
            for (int h = 0; h < 2 && tag_left_ != 0; ++h) {
                const uint32_t d = bits(tag_regs_, static_cast<int>(4 * tag_reg_), 4);
                if (d != 0xe && d != 0xf) write_reg(d, r64(q + 8 * h));
                tag_reg_ = (tag_reg_ + 1) % tag_nreg_;
                --tag_left_;
            }
        } else {
            const uint32_t take = std::min(tag_left_, qwc - i);
            mem_.transfer_data(regs_[0x50], regs_[0x51], regs_[0x52], q, take * 16);
            tag_left_ -= take;
            i += take - 1;
        }
    }
}

void Gs::vif_stream(const uint8_t* p, uint32_t n, bool) {
    uint32_t i = 0;
    while (i + 4 <= n) {
        const uint32_t code = r32(p + i);
        i += 4;
        const uint32_t cmd = (code >> 24) & 0x7f, imm = code & 0xffff;
        static const bool vif_trace = std::getenv("RAC1_VIF_TRACE") != nullptr;  // testing: the VIF1 codes
        if (vif_trace) std::fprintf(stderr, "[VIF1] %08x\n", code);
        static const bool mark_trace = std::getenv("RAC1_VU1_TRACE") != nullptr;  // testing: the passes' MARKs in the VU1 trace
        if (mark_trace && cmd == 0x07) std::fprintf(stderr, "[VU1] MARK %04x\n", imm);
        if (dry_) return;  // (host, a skipped tag: its VIF codes and data are not processed)
        if (cmd == 0x50 || cmd == 0x51) {  // DIRECT / DIRECTHL: GIF data (qwords)
            const uint32_t q = imm == 0 ? 65536 : imm;
            i = (i + 15) & ~15u;
            const uint32_t take = std::min(q * 16, n - std::min(n, i));
            gif(p + i, take / 16);
            i += q * 16;
        } else {  // the VU1 side (cycle, masks, UNPACK, MPG, double buffer, program starts)
            vu1::Vu1& vu = vu1::vu1_global();
            const uint32_t nb = vu1::vif_data_bytes(vu, code);
            if (nb == 0 || i + nb <= n) vu1::vif1_command(vu, code, p + i, [this](const uint8_t* d, uint32_t q) {
                vu_kick_ = true;
                gif(d, q);
                vu_kick_ = false;
            });
            i += nb;
        }
        // NOP, MSKPATH3, MARK, FLUSH*: nothing to do
    }
}

void Gs::add_chain_substitute(uint32_t first, uint32_t end, std::vector<uint8_t> chain) {
    Sub u;
    u.first = first & 0x01fffff0u;
    u.end = end & 0x01fffff0u;
    u.chain = std::move(chain);
    if (u.chain.empty() || u.first == 0 || static_cast<int>(subs_.size()) >= kMaxSubs) return;
    subs_.push_back(std::move(u));
}
uint32_t Gs::sub_size_of(int k) const {
    if (k < 0 || k >= static_cast<int>(subs_.size())) return 0;
    const Sub& u = subs_[static_cast<size_t>(k)];
    return u.replay_size != 0xffffffffu ? u.replay_size : static_cast<uint32_t>(u.chain.size());
}
// (host) EE memory, or substitute k's bytes for an address that carries kSubFlag (and k)
const uint8_t* Gs::sub_mem(uint32_t a) const {
    if ((a & kSubFlag) == 0) return static_cast<const uint8_t*>(ee_ram_ptr_read(a));
    const size_t k = (a >> kSubShift) & 7u;
    if (k >= subs_.size()) return nullptr;
    const Sub& u = subs_[k];
    const uint32_t o = (a & 0x01ffffffu) - u.first;
    return o < u.chain.size() ? u.chain.data() + o : nullptr;
}

// (host) see gs_emu.h
void Gs::set_chain_fx(std::vector<FxRange> ranges, bool external) {
    std::sort(ranges.begin(), ranges.end(), [](const FxRange& a, const FxRange& b) { return a.sub != b.sub ? a.sub < b.sub : a.first < b.first; });
    fx_ranges_ = std::move(ranges);
    fx_external_ = external && !fx_ranges_.empty();
}
// (the range holding the tag at `addr`, by a binary search: a frame has hundreds of them, one per effect quad)
const Gs::FxRange* Gs::fx_tag(uint32_t addr, uint8_t sub) const {
    auto it = std::upper_bound(fx_ranges_.begin(), fx_ranges_.end(), std::make_pair(sub, addr), [](const std::pair<uint8_t, uint32_t>& k, const FxRange& r) {
        return k.first != r.sub ? k.first < r.sub : k.second < r.first;
    });
    if (it == fx_ranges_.begin()) return nullptr;
    --it;
    return it->sub == sub && addr >= it->first && addr <= it->last ? &*it : nullptr;
}

// (host) see gs_emu.h
void Gs::set_chain_layers(std::vector<std::pair<uint32_t, uint32_t>> ranges) {
    std::sort(ranges.begin(), ranges.end());
    layer_ranges_ = std::move(ranges);
}

// (host) see gs_emu.h
void Gs::set_chain_vu7(std::vector<Vu7Range> ranges, uint64_t hash, uint8_t kind) {
    std::sort(ranges.begin(), ranges.end(), [](const Vu7Range& a, const Vu7Range& b) { return a.sub != b.sub ? a.sub < b.sub : a.first < b.first; });
    vu7_ranges_ = std::move(ranges);
    vu7_hash_ = hash;
    vu7_kind_ = kind;
    if (!vu7_ranges_.empty() && kind != 0) fx_external_ = true;
}

// (host) see gs_emu.h: before a program start on the walk's VU1 - a batch of program 7 in a vu7 range kept as the program finds it
// (its GIF tag, then per vertex ST, RGBA, XYZ at XTOP; the constants the start-up loaded: vf18..vf21, vf30, vf31.w, vf7), with the
// GS state its kick would have, and its place in the primitives' order
void Gs::vu1_before(void* vu_p, uint32_t start, uint64_t hash) {
    vu1::Vu1& vu = *static_cast<vu1::Vu1*>(vu_p);
    if (!vu7_now_ || hash != vu7_hash_ || vu7_kind_ == 0 || (start != 0xc && start != 0xe)) return;
    const VUState& st = vu.st;
    auto q = [&st](uint32_t a) { return &st.mem[(a & 0x3ffu) << 4]; };
    const uint32_t x = st.xtop;
    uint64_t tag;
    std::memcpy(&tag, q(x), 8);
    const uint32_t n = static_cast<uint32_t>(tag & 0x7fff);
    if (n < 3 || n > 0x150) return;  // (a batch holds at most (0x1d2 - 1) / 3 vertices)
    Vu7Batch b;
    // the GS state a kick under a GIF tag `t` would have now (its PRIM when PRE, the context's registers)
    auto state_of = [this](uint64_t t, DrawState& s, std::shared_ptr<const TexImage>& tex, bool* mip) {
        const uint64_t prim0 = bits(t, 46, 1) != 0 ? bits(t, 47, 11) : regs_[0x00];
        const uint64_t prim = bits(regs_[0x1a], 0, 1) ? prim0 : (regs_[0x1b] & ~7ull) | (prim0 & 7);
        const int ctx = bits(prim, 9, 1);
        s.prim = prim;
        s.tex0 = regs_[0x06 + ctx];
        s.clamp = regs_[0x08 + ctx];
        s.tex1 = regs_[0x14 + ctx];
        s.miptbp1 = regs_[0x34 + ctx];
        s.alpha = regs_[0x42 + ctx];
        s.test = regs_[0x47 + ctx];
        s.scissor = regs_[0x40 + ctx];
        s.fba = regs_[0x4a + ctx];
        s.frame = regs_[0x4c + ctx];
        s.zbuf = regs_[0x4e + ctx];
        s.texa = regs_[0x3b];
        s.colclamp = regs_[0x46];
        s.pabe = regs_[0x49];
        s.fogcol = regs_[0x3d];
        s.dthe = regs_[0x45];
        s.xyoffset = regs_[0x18 + ctx];
        if (bits(prim, 4, 1) != 0) {
            tex = mem_.texture_at_kick(s.tex0, s.texa);
            if (mip != nullptr) *mip = std::min(bits(s.tex1, 2, 3), 3u) != 0 && bits(s.tex1, 6, 3) >= 2;
        }
    };
    state_of(tag, b.st, b.tex, &b.mip);
    if (start == 0xe) {
        uint64_t ft;
        std::memcpy(&ft, &st.vf[31].u[0], 8);
        state_of(ft, b.st_fan, b.tex_fan, nullptr);
    }
    const DrawState& s = b.st;
    for (int c = 0; c < 4; ++c)
        for (int l = 0; l < 4; ++l) b.m[4 * c + l] = st.vf[static_cast<size_t>(18 + c)].f[l];
    for (int c = 0; c < 4; ++c)
        for (int l = 0; l < 4; ++l) b.clip[4 * c + l] = st.vf[static_cast<size_t>(22 + c)].f[l];
    for (int l = 0; l < 4; ++l) b.off[l] = st.vf[30].f[l];
    b.k = st.vf[31].f[3];
    b.fog[0] = st.vf[7].f[0];
    b.fog[1] = st.vf[7].f[1];
    b.first = static_cast<uint32_t>(vu7_verts_.size());
    b.n = n;
    b.start = start;
    b.tag = vu7_tag_;
    b.sub = sub_wide_ ? 1 : sub_drop_ ? 2 : 0;
    for (uint32_t i = 0; i < n; ++i) {
        Vu7Vertex v;
        float f[4];
        uint32_t u[4];
        std::memcpy(f, q(x + 1 + 3 * i), 16);
        v.st[0] = f[0];
        v.st[1] = f[1];
        v.st[2] = f[2];
        std::memcpy(u, q(x + 2 + 3 * i), 16);
        for (int l = 0; l < 4; ++l) v.rgba[l] = static_cast<uint8_t>(u[l]);
        std::memcpy(f, q(x + 3 + 3 * i), 16);
        v.xyz[0] = f[0];
        v.xyz[1] = f[1];
        v.xyz[2] = f[2];
        vu7_verts_.push_back(v);
    }
    if (scr_prim_ >= 0) screen_picture();
    Primitive p;  // (its place in the primitives' order)
    p.type = 9;
    p.st = s;
    p.fx = vu7_kind_;
    p.fx_id = static_cast<uint32_t>(vu7_batches_.size());
    p.sub = b.sub;
    p.wide = sub_wide_;
    p.vu = true;
    p.vu_start = start;
    prims_.push_back(std::move(p));
    vu7_batches_.push_back(std::move(b));
    vu7_drop_ = true;
}

// (host) see gs_emu.h
void Gs::vu1_after(void*, uint32_t, uint64_t) { vu7_drop_ = false; }

// (host) see gs_emu.h
void Gs::set_chain_hud(std::vector<HudRange> ranges) {
    std::sort(ranges.begin(), ranges.end(), [](const HudRange& a, const HudRange& b) { return a.first < b.first; });
    hud_ranges_ = std::move(ranges);
}

void Gs::set_chain_skips(std::vector<SkipRange> ranges) {
    std::sort(ranges.begin(), ranges.end(), [](const SkipRange& a, const SkipRange& b) { return a.first < b.first; });
    skip_ranges_ = std::move(ranges);
    skip_started_.assign(skip_ranges_.size(), false);
}

// (host) whether the tag at `addr` is in a skip range (the last range starting at or before it)
bool Gs::tag_skipped(uint32_t addr, size_t* range) const {
    if (skip_ranges_.empty()) return false;
    auto it = std::upper_bound(skip_ranges_.begin(), skip_ranges_.end(), addr, [](uint32_t a, const SkipRange& r) { return a < r.first; });
    while (it != skip_ranges_.begin()) {
        --it;
        if (addr <= it->last) {
            if (range != nullptr) *range = static_cast<size_t>(it - skip_ranges_.begin());
            return true;
        }
        if (addr - it->first > 0x100000u) break;  // (ranges are short: no need to look further back)
    }
    return false;
}

// The chain walked (its DMA tags followed) by `mem` (the tag's or the data's EE address and length -> the bytes, nullptr: none).
template <class Mem>
void Gs::chain_walk(uint32_t tag, uint32_t end, Mem&& mem) {
    struct ClearSkips {  // (host) the skip ranges last for one chain
        Gs& g;
        ~ClearSkips() {
            g.skip_ranges_.clear();
            g.fx_ranges_.clear();
            g.fx_now_ = 0;
            g.layer_ranges_.clear();
            g.layer_now_ = false;
            g.hud_ranges_.clear();
            g.hud_now_ = 0;
            g.vu7_ranges_.clear();
            g.vu7_now_ = false;
            g.vu7_drop_ = false;
        }
    } clear_skips{*this};
    struct Observe : vu1::StartObserver {  // (host) the program starts of this walk (vu1_before / vu1_after), on this thread
        Gs& g;
        vu1::StartObserver* before_obs;
        explicit Observe(Gs& gs) : g(gs), before_obs(vu1::g_start_observer) { vu1::g_start_observer = this; }
        ~Observe() override { vu1::g_start_observer = before_obs; }
        void before(vu1::Vu1& vu, uint32_t start, uint64_t hash) override { g.vu1_before(&vu, start, hash); }
        void after(vu1::Vu1& vu, uint32_t start, uint64_t hash) override { g.vu1_after(&vu, start, hash); }
    } observe{*this};
    struct ClearSub {  // (host) the substitutes last for one chain
        Gs& g;
        ~ClearSub() {
            g.subs_.clear();
            g.mem_.undo_end();
            g.sub_wide_ = g.sub_drop_ = false;
        }
    } clear_sub{*this};
    struct SideWiden {  // (host) the view's widening for the VU1 programs this walk runs (set_side_widen), on this thread
        float before[2];
        explicit SideWiden(const float w[2]) : before{vu1::g_side_widen[0], vu1::g_side_widen[1]} {
            vu1::g_side_widen[0] = w[0] > 1.0f ? w[0] : 1.0f;
            vu1::g_side_widen[1] = w[1] > 1.0f ? w[1] : 1.0f;
        }
        ~SideWiden() {
            vu1::g_side_widen[0] = before[0];
            vu1::g_side_widen[1] = before[1];
        }
    } side_widen{side_widen_};
    std::vector<uint32_t> stack;
    std::vector<uint8_t> buf;
    uint32_t addr = tag & 0x01fffff0u;
    // inside substitute `cur`: an address in [first, first + size) is the substitute's (kSubFlag)
    bool in_sub = false;
    int cur = -1;
    uint32_t sub_lo = 0, sub_hi = 0;
    uint32_t done = 0;  // (the substitutes walked)
    auto sub_addr = [&](uint32_t a) { return in_sub && a >= sub_lo && a < sub_hi ? sub_flagged(a, cur) : a; };
    vu1::Vu1 vu_saved;
    struct GsSaved {  // the GS / VIF / GIF state the substitute's walk changes (its uploads into the local memory: vundo_)
        uint64_t regs[0x80];
        Vertex cur;
        std::vector<Vertex> queue;
        LocalMemory::TransferState xfer;
        uint32_t tag_left, tag_flg, tag_nreg, tag_reg;
        uint64_t tag_regs;
        uint32_t cl, wl;
    };
    std::unique_ptr<GsSaved> gs_saved;
    for (int guard = 0; guard < 200000; ++guard) {
        if (!in_sub && !sub_drop_)  // a substitute first, from the real range's first tag
            for (int k = 0; k < static_cast<int>(subs_.size()); ++k)
                if ((done >> k & 1u) == 0 && addr == subs_[static_cast<size_t>(k)].first) {
                    cur = k;
                    sub_lo = subs_[static_cast<size_t>(k)].first;
                    sub_hi = sub_lo + sub_size_of(k);
                    break;
                }
        if (cur >= 0 && !in_sub && !sub_drop_ && (done >> cur & 1u) == 0 && addr == sub_lo) {
            vu_saved = vu1::vu1_global();
            mem_.undo_begin();
            gs_saved.reset(new GsSaved{{}, cur_, queue_, mem_.transfer_state(), tag_left_, tag_flg_, tag_nreg_, tag_reg_, tag_regs_, vif_cl_, vif_wl_});
            std::memcpy(gs_saved->regs, regs_, sizeof regs_);
            in_sub = sub_wide_ = true;
            sub_prims0_ = prims_.size();
            sub_dropped_ = 0;
            stack.clear();
        }
        if (in_sub && addr == sub_hi) {  // its end: the state put back, the real range walked (its primitives marked sub 2)
            vu1::vu1_global() = vu_saved;
            mem_.undo_restore();  // (the local memory as before the substitute: the real range's textures are its own)
            const GsSaved& g = *gs_saved;
            std::memcpy(regs_, g.regs, sizeof regs_);
            cur_ = g.cur;
            queue_ = g.queue;
            mem_.set_transfer_state(g.xfer);
            tag_left_ = g.tag_left;
            tag_flg_ = g.tag_flg;
            tag_nreg_ = g.tag_nreg;
            tag_reg_ = g.tag_reg;
            tag_regs_ = g.tag_regs;
            vif_cl_ = g.cl;
            vif_wl_ = g.wl;
            in_sub = sub_wide_ = false;
            sub_drop_ = true;
            addr = sub_lo;
            stack.clear();
        }
        if (sub_drop_ && addr == subs_[static_cast<size_t>(cur)].end) {
            sub_drop_ = false;
            done |= 1u << cur;
            static const bool trace = std::getenv("RAC1_SUB_TRACE") != nullptr;  // (testing)
            if (trace) std::fprintf(stderr, "[sub] %d %08x..%08x: substitute + real %zu prims, real %llu\n", cur, sub_lo, subs_[static_cast<size_t>(cur)].end, prims_.size() - sub_prims0_, static_cast<unsigned long long>(sub_dropped_));
            cur = -1;
        }
        if (end != 0 && !in_sub && addr == (end & 0x01fffff0u)) return;  // (the substitute may reach past the real chain's end)
        const uint8_t* t = mem(sub_addr(addr), 16u);
        if (t == nullptr) return;
        const uint64_t lo = r64(t);
        const uint32_t qwc = bits(lo, 0, 16), id = bits(lo, 28, 3);
        const uint32_t a = bits(lo, 32, 31) & 0x01fffff0u;
        uint32_t data = addr + 16, next = addr + 16 + qwc * 16;
        bool stop = false;
        switch (id) {
        case 0: data = a; stop = true; break;               // REFE
        case 1: break;                                      // CNT
        case 2: next = a; break;                            // NEXT
        case 3: case 4: data = a; next = addr + 16; break;  // REF / REFS
        case 5: stack.push_back(next); next = a; break;     // CALL
        case 6:                                             // RET
            if (stack.empty()) stop = true;
            else { next = stack.back(); stack.pop_back(); }
            break;
        case 7: stop = true; break;  // END
        }
        static const bool vif_trace = std::getenv("RAC1_VIF_TRACE") != nullptr;
        if (vif_trace) std::fprintf(stderr, "[DMA] tag @%08x id %u qwc %u data %08x\n", addr, id, qwc, data);
        static const bool vu1_trace = std::getenv("RAC1_VU1_TRACE") != nullptr;  // testing: where in the chain the VU1 events are
        if (vu1_trace) std::fprintf(stderr, "[VU1] tag @%08x data %08x qwc %u\n", addr, data, qwc);
        // the tag's VIF codes (its upper 64 bits, stream offsets 8 and 12; its lower half, not
        // sent to the VIF, as two NOPs) then its data from the next quadword, one stream
        size_t k = 0;
        dry_ = !in_sub && tag_skipped(addr, &k);  // (the skip ranges are the real chain's)
        {  // (the MRP's effects)
            const FxRange* fr = fx_ranges_.empty() ? nullptr : fx_tag(addr, in_sub ? static_cast<uint8_t>(cur + 1) : 0);
            fx_now_ = fr != nullptr ? fr->kind : 0;
            fx_id_ = fr != nullptr ? fr->id : 0;
        }
        if (fx_now_ != 0 && fx_mark_ == SIZE_MAX) fx_mark_ = prims_.size();
        layer_now_ = false;
        if (!layer_ranges_.empty() && !in_sub) {  // (a screen layer's: the real chain's tags)
            auto it = std::upper_bound(layer_ranges_.begin(), layer_ranges_.end(), addr,
                                       [](uint32_t a, const std::pair<uint32_t, uint32_t>& r) { return a < r.first; });
            layer_now_ = it != layer_ranges_.begin() && addr <= std::prev(it)->second;
        }
        vu7_now_ = false;
        if (!vu7_ranges_.empty()) {  // (the MRP's program-7 surfaces)
            const uint8_t sb = in_sub ? static_cast<uint8_t>(cur + 1) : 0;
            auto it = std::upper_bound(vu7_ranges_.begin(), vu7_ranges_.end(), std::make_pair(sb, addr), [](const std::pair<uint8_t, uint32_t>& k, const Vu7Range& r) {
                return k.first != r.sub ? k.first < r.sub : k.second < r.first;
            });
            if (it != vu7_ranges_.begin()) {
                --it;
                vu7_now_ = it->sub == sb && addr >= it->first && addr <= it->last;
                vu7_tag_ = it->tag;
            }
        }
        hud_now_ = 0;
        if (!hud_ranges_.empty() && !in_sub) {  // (a HUD element's: the real chain's tags)
            auto it = std::upper_bound(hud_ranges_.begin(), hud_ranges_.end(), addr, [](uint32_t a, const HudRange& r) { return a < r.first; });
            if (it != hud_ranges_.begin() && addr <= std::prev(it)->last) hud_now_ = std::prev(it)->side;
        }
        if (dry_) {
            ++skipped_tags_;
            if (!skip_started_[k]) {  // the range's VU1 program starts, once
                skip_started_[k] = true;
                if (skip_ranges_[k].moby && moby_mark_ == SIZE_MAX) moby_mark_ = prims_.size();
                if (skip_ranges_[k].host_3d != 0) {  // the host's 3D in the primitives' order, with context 1's state now
                    if (scr_prim_ >= 0) screen_picture();
                    Primitive p;
                    p.type = 8;
                    DrawState& s = p.st;
                    s.prim = regs_[0x00];
                    s.alpha = regs_[0x42];
                    s.test = regs_[0x47];
                    s.scissor = regs_[0x40];
                    s.fba = regs_[0x4a];
                    s.frame = regs_[0x4c];
                    s.zbuf = regs_[0x4e];
                    s.xyoffset = regs_[0x18];
                    p.fx_id = skip_ranges_[k].host_3d;
                    prims_.push_back(std::move(p));
                }
                vu1::Vu1& vu = vu1::vu1_global();
                for (uint16_t m : skip_ranges_[k].mscals)
                    vu1::vif1_command(vu, 0x14000000u | m, nullptr, [this](const uint8_t* d, uint32_t q) {
                        vu_kick_ = true;
                        gif(d, q);
                        vu_kick_ = false;
                    });
            }
        }
        buf.assign(16, 0);
        std::memcpy(buf.data() + 8, t + 8, 8);
        if (qwc != 0 && !dry_) {  // (a skipped tag's data is not read: vif_stream stops at its first code)
            const uint8_t* d = mem(sub_addr(data), qwc * 16);
            if (d != nullptr) buf.insert(buf.end(), d, d + qwc * 16);
        }
        vif_stream(buf.data(), static_cast<uint32_t>(buf.size()), true);
        dry_ = false;
        if (stop) return;
        addr = next;
    }
}

void Gs::vif1_chain(uint32_t tag, uint32_t end) {
    chain_walk(tag, end, [this](uint32_t a, uint32_t) { return sub_mem(a); });
    if (scr_prim_ >= 0) screen_picture();
}

// an IMAGE transfer into the local memory at block `dbp` begins: into the frame, a type-7 primitive (one for transfers in a row)
void Gs::screen_transfer(uint32_t dbp) {
    const uint32_t blocks = scr_bw_ * 64 * scr_h_ * 4 / 256;
    if (dbp < scr_bp_ || dbp >= scr_bp_ + blocks || scr_prim_ >= 0) return;
    Primitive p;
    p.type = 7;
    p.v.resize(2);
    p.v[1].x = static_cast<float>(scr_bw_ * 64);
    p.v[1].y = static_cast<float>(scr_h_);
    p.st.frame = (scr_bp_ / 32) | (static_cast<uint64_t>(scr_bw_) << 16);
    scr_prim_ = static_cast<long>(prims_.size());
    prims_.push_back(std::move(p));
}

// the type-7 primitive's picture: the frame's memory now
void Gs::screen_picture() {
    prims_[static_cast<size_t>(scr_prim_)].tex = mem_.picture_ct32(scr_bp_, scr_bw_, scr_bw_ * 64, scr_h_);
    scr_prim_ = -1;
}

// the tags and the data a vif1_chain(tag, end) would read, in its order (the skipped tags' data not)
void Gs::capture_chain(uint32_t tag, uint32_t end, ChainCapture& c) const {
    c.tag = tag;
    c.end = end;
    c.size = 0;
    c.spans.clear();
    c.subs.clear();
    c.side_widen[0] = side_widen_[0];
    c.side_widen[1] = side_widen_[1];
    for (int k = 0; k < static_cast<int>(subs_.size()); ++k) c.subs.push_back({subs_[static_cast<size_t>(k)].first, subs_[static_cast<size_t>(k)].end, sub_size_of(k)});
    auto take = [&c, this](uint32_t a, uint32_t len) -> const uint8_t* {
        const uint8_t* src = sub_mem(a);
        const size_t at = c.size;
        c.spans.push_back({a, static_cast<uint32_t>(at), src == nullptr ? 0xffffffffu : len});
        if (src == nullptr) return nullptr;
        if (at + len > c.cap) {
            const size_t ncap = std::max<size_t>(at + len, c.cap * 2 + 65536);
            std::unique_ptr<uint8_t[]> nb(new uint8_t[ncap]);
            if (at != 0) std::memcpy(nb.get(), c.bytes.get(), at);
            c.bytes = std::move(nb);
            c.cap = ncap;
        }
        std::memcpy(c.bytes.get() + at, src, len);
        c.size = at + len;
        return c.bytes.get() + at;
    };
    std::vector<uint32_t> stack;
    uint32_t addr = tag & 0x01fffff0u;
    // the substitute walked as chain_walk walks it (the same addresses asked for, in the same order)
    bool in_sub = false, drop = false;
    int cur = -1;
    uint32_t sub_lo = 0, sub_hi = 0, done = 0;
    auto sub_addr = [&](uint32_t a) { return in_sub && a >= sub_lo && a < sub_hi ? sub_flagged(a, cur) : a; };
    for (int guard = 0; guard < 200000; ++guard) {
        if (!in_sub && !drop)
            for (int k = 0; k < static_cast<int>(subs_.size()); ++k)
                if ((done >> k & 1u) == 0 && addr == subs_[static_cast<size_t>(k)].first) {
                    cur = k;
                    sub_lo = subs_[static_cast<size_t>(k)].first;
                    sub_hi = sub_lo + sub_size_of(k);
                    in_sub = true;
                    stack.clear();
                    break;
                }
        if (in_sub && addr == sub_hi) {
            in_sub = false;
            drop = true;
            addr = sub_lo;
            stack.clear();
        }
        if (drop && addr == subs_[static_cast<size_t>(cur)].end) {
            drop = false;
            done |= 1u << cur;
            cur = -1;
        }
        if (end != 0 && !in_sub && addr == (end & 0x01fffff0u)) return;  // (the substitute may reach past the real chain's end)
        const uint8_t* t = take(sub_addr(addr), 16u);
        if (t == nullptr) return;
        const uint64_t lo = r64(t);
        const uint32_t qwc = bits(lo, 0, 16), id = bits(lo, 28, 3);
        const uint32_t a = bits(lo, 32, 31) & 0x01fffff0u;
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
        if (qwc != 0 && (in_sub || !tag_skipped(addr, nullptr))) take(sub_addr(data), qwc * 16);
        if (stop) return;
        addr = next;
    }
}

// vif1_chain on a capture (any thread: no EE memory read)
void Gs::vif1_chain(const ChainCapture& c) {
    size_t at = 0;
    subs_.clear();  // (the substitutes' addresses as the capture saw them; their bytes are in the capture)
    for (const auto& u : c.subs) {
        Sub v;
        v.first = u[0];
        v.end = u[1];
        v.replay_size = u[2];
        subs_.push_back(std::move(v));
    }
    side_widen_[0] = c.side_widen[0];
    side_widen_[1] = c.side_widen[1];
    chain_walk(c.tag, c.end, [&c, &at](uint32_t a, uint32_t) -> const uint8_t* {
        if (at < c.spans.size() && c.spans[at][0] == a) {
            const auto& sp = c.spans[at++];
            return sp[2] == 0xffffffffu ? nullptr : c.bytes.get() + sp[1];
        }
        return static_cast<const uint8_t*>(ee_ram_ptr_read(a));  // (never: the capture follows the same tags)
    });
    if (scr_prim_ >= 0) screen_picture();
}


namespace {
// the chain's run on a worker thread
struct ChainWorker {
    std::mutex m;
    std::condition_variable cv;
    std::thread th;
    bool job = false, busy = false, quit = false;
    const Gs::ChainCapture* cap = nullptr;
    std::exception_ptr error;
    void loop() {
        std::unique_lock<std::mutex> lk(m);
        for (;;) {
            cv.wait(lk, [this] { return job || quit; });
            if (quit) return;
            job = false;
            lk.unlock();
            try {
                const rac1::prof::Job prof_job(rac1::prof::kWorkerChain);  // (the debug menu's profiler: the job's time)
                gs_global().vif1_chain(*cap);
            } catch (...) {
                error = std::current_exception();
            }
            lk.lock();
            busy = false;
            cv.notify_all();
        }
    }
    ~ChainWorker() {
        {
            std::lock_guard<std::mutex> lk(m);
            quit = true;
        }
        cv.notify_all();
        if (th.joinable()) th.join();
    }
} g_chain_worker;
}  // namespace

void chain_start_async(const Gs::ChainCapture& c) {
    add_memory_join(&chain_join);
    ChainWorker& w = g_chain_worker;
    chain_join();
    std::lock_guard<std::mutex> lk(w.m);
    if (!w.th.joinable()) w.th = std::thread([&w] {
        rac1::prof::thread_name("GS chain worker");
        w.loop();
    });
    w.cap = &c;
    w.error = nullptr;
    w.busy = true;
    w.job = true;
    w.cv.notify_all();
}

void chain_join() {
    RAC1_PROF_ZONE(kChainJoinWait);
    ChainWorker& w = g_chain_worker;
    std::unique_lock<std::mutex> lk(w.m);
    w.cv.wait(lk, [&w] { return !w.busy; });
    if (w.error) {
        std::exception_ptr e = w.error;
        w.error = nullptr;
        std::rethrow_exception(e);
    }
}

}  // namespace rac1::gs
