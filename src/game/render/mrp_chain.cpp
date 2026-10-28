// See mrp_chain.h.
#include <game/render/mrp_chain.h>

#include <game/render/mrp_fx.h>
#include <game/render/render_toggles.h>
#include <profiler/host_profiler.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/gs/gs_memory.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/vu/vu1.h>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <thread>

#if defined(__WIIU__)
#include <coreinit/core.h>
#include <coreinit/thread.h>
#endif

namespace rac1 {

namespace {

using gs::DrawState;
using gs::Primitive;
using gs::TexImage;
using gs::Vertex;
using gs::Vu7Batch;
using gs::Vu7Vertex;

// orig: (host) a doubleword of EE / VU memory: host-native 32-bit words, the low one first (ee_memory.h; the same on a
// little-endian host)
uint64_t r64(const uint8_t* p) {
    uint32_t w[2];
    std::memcpy(w, p, 8);
    return (static_cast<uint64_t>(w[1]) << 32) | w[0];
}
// orig: (host) the PS2's byte i of a word-aligned block of EE memory
uint8_t byte_at(const uint8_t* p, uint32_t i) { return p[i ^ EE_BYTE_SWIZZLE]; }
// orig: (host)
uint32_t r32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
// orig: (host)
float f32(uint32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
uint32_t bits(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

constexpr uint32_t kSubFlag = 0x80000000u;
constexpr int kSubShift = 26;
constexpr int kMaxSubs = 4;
uint32_t sub_flagged(uint32_t a, int k) { return a | kSubFlag | (static_cast<uint32_t>(k) << kSubShift); }

// A frame's chain as the game's thread copied it: the input, and the bytes the reading asks for, in the order it asks
// (the tags, then each tag's data; the skipped tags' data not taken).
struct Capture {
    MrpChainInput in;
    std::vector<uint8_t> bytes;
    std::vector<std::array<uint32_t, 3>> spans;  // (address as asked, offset, length; 0xffffffff: none)
};

// The skip ranges sorted by `first`: the range holding the tag at `addr` (-1: none)
// orig: (host)
long skip_of(const std::vector<MrpChainInput::Skip>& sk, uint32_t addr) {
    if (sk.empty()) return -1;
    auto it = std::upper_bound(sk.begin(), sk.end(), addr, [](uint32_t a, const MrpChainInput::Skip& r) { return a < r.first; });
    while (it != sk.begin()) {
        --it;
        if (addr <= it->last) return static_cast<long>(it - sk.begin());
        if (addr - it->first > 0x100000u) break;  // (ranges are short: no need to look further back)
    }
    return -1;
}

// The walk of the chain's tags (shared by the capture and the reading, so that both ask for the same bytes in the same order):
// `on_tag(addr as asked, tag bytes, in_sub, sub index, sub state)` reads a tag, `data(addr as asked, length)` its data.
// The substitutes as the GS replica walks them: a substitute k from its real range's first tag, then the real range again.
struct Walk {
    const MrpChainInput& in;
    explicit Walk(const MrpChainInput& i) : in(i) {}
    template <class Mem, class OnTag>
    void run(Mem&& mem, OnTag&& on_tag) {
        std::vector<uint32_t> stack;
        uint32_t addr = in.start & 0x01fffff0u;
        bool in_sub = false, drop = false;
        int cur = -1;
        uint32_t sub_lo = 0, sub_hi = 0, done = 0;
        const int nsubs = std::min(static_cast<int>(in.subs.size()), kMaxSubs);
        auto sub_addr = [&](uint32_t a) { return in_sub && a >= sub_lo && a < sub_hi ? sub_flagged(a, cur) : a; };
        for (int guard = 0; guard < 200000; ++guard) {
            int event = 0;  // 1: a substitute starts here, 2: it ended (the real range from here), 3: the real range ended
            if (!in_sub && !drop)
                for (int k = 0; k < nsubs; ++k)
                    if ((done >> k & 1u) == 0 && addr == in.subs[static_cast<size_t>(k)].first) {
                        cur = k;
                        sub_lo = in.subs[static_cast<size_t>(k)].first;
                        sub_hi = sub_lo + static_cast<uint32_t>(in.subs[static_cast<size_t>(k)].bytes.size());
                        in_sub = true;
                        stack.clear();
                        event = 1;
                        break;
                    }
            if (in_sub && addr == sub_hi) {
                in_sub = false;
                drop = true;
                addr = sub_lo;
                stack.clear();
                event = 2;
            }
            if (drop && addr == in.subs[static_cast<size_t>(cur)].end) {
                drop = false;
                done |= 1u << cur;
                cur = -1;
                event = event == 0 ? 3 : event;
            }
            if (in.end != 0 && !in_sub && addr == (in.end & 0x01fffff0u)) return;  // (the substitute may reach past the real chain's end)
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
            const long skip = in_sub ? -1 : skip_of(in.skips, addr);
            const uint8_t* d = nullptr;
            if (qwc != 0 && (skip < 0 || !in.skips[static_cast<size_t>(skip)].dry)) d = mem(sub_addr(data), qwc * 16);
            on_tag(addr, t, d, qwc, skip, in_sub, drop, cur, event);
            if (stop) return;
            addr = next;
        }
    }
    // (EE memory, or substitute k's bytes for an address that carries kSubFlag and k)
    const uint8_t* sub_mem(uint32_t a) const {
        if ((a & kSubFlag) == 0) return static_cast<const uint8_t*>(ee_ram_ptr_read(a));
        const size_t k = (a >> kSubShift) & 7u;
        if (k >= in.subs.size()) return nullptr;
        const auto& u = in.subs[k];
        const uint32_t o = (a & 0x01ffffffu) - u.first;
        return o < u.bytes.size() ? u.bytes.data() + o : nullptr;
    }
};

// The reader: the GS registers and the GIF's state, the VU1 the effects' programs run on, the frame read.
class Reader : public vu1::StartObserver {
public:
    MrpChainFrame frame;
    uint64_t regs[0x80] = {};

    void read(const Capture& c) {
        const MrpChainInput& in = c.in;
        in_ = &in;
        frame.prims.clear();
        frame.vu7_batches.clear();
        frame.vu7_verts.clear();
        frame.fx_mark = frame.moby_mark = SIZE_MAX;
        frame.fx_external = !in.fx.empty() || (!in.vu7.empty() && in.vu7_kind != 0);
        frame.tags = frame.skipped_tags = frame.program_runs = frame.flips = frame.dropped_3d = 0;
        frame.regs_at_ok = false;
        tag_left_ = 0;
        scr_prim_ = -1;
        skip_started_.assign(in.skips.size(), false);
        struct Observe {  // (program 7's starts: its batches taken before it runs; on this thread)
            vu1::StartObserver* before;
            explicit Observe(vu1::StartObserver* o) : before(vu1::g_start_observer) { vu1::g_start_observer = o; }
            ~Observe() { vu1::g_start_observer = before; }
        } observe{this};
        struct SideWiden {  // (the view's widening for the VU1 programs this reading runs, on this thread)
            float before[2];
            explicit SideWiden(const float w[2]) : before{vu1::g_side_widen[0], vu1::g_side_widen[1]} {
                vu1::g_side_widen[0] = w[0] > 1.0f ? w[0] : 1.0f;
                vu1::g_side_widen[1] = w[1] > 1.0f ? w[1] : 1.0f;
            }
            ~SideWiden() {
                vu1::g_side_widen[0] = before[0];
                vu1::g_side_widen[1] = before[1];
            }
        } side_widen{in.side_widen};
        gs::LocalMemory& mem = gs::local_memory();
        // the state a substitute's reading changes, put back before its real range (the uploads: the memory's undo)
        struct Saved {
            vu1::Vu1 vu;
            uint64_t regs[0x80];
            Vertex cur;
            std::vector<Vertex> queue;
            gs::LocalMemory::TransferState xfer;
            uint32_t tag_left, tag_flg, tag_nreg, tag_reg;
            uint64_t tag_regs;
        };
        std::unique_ptr<Saved> saved;
        size_t at = 0;
        Walk w(in);
        w.run(
            [&c, &at](uint32_t a, uint32_t) -> const uint8_t* {
                if (at < c.spans.size() && c.spans[at][0] == a) {
                    const auto& sp = c.spans[at++];
                    return sp[2] == 0xffffffffu ? nullptr : c.bytes.data() + sp[1];
                }
                return nullptr;  // (never: the capture followed the same tags)
            },
            [&](uint32_t addr, const uint8_t* t, const uint8_t* d, uint32_t qwc, long skip, bool in_sub, bool drop, int cur, int event) {
                if (event == 1) {  // a substitute's reading starts: the state kept
                    saved.reset(new Saved{vu_, {}, cur_, queue_, mem.transfer_state(), tag_left_, tag_flg_, tag_nreg_, tag_reg_, tag_regs_});
                    std::memcpy(saved->regs, regs, sizeof regs);
                    mem.undo_begin();
                } else if (event == 2 && saved != nullptr) {  // its end: the state put back, the real range read
                    mem.undo_restore();
                    vu_ = saved->vu;
                    std::memcpy(regs, saved->regs, sizeof regs);
                    cur_ = saved->cur;
                    queue_ = saved->queue;
                    mem.set_transfer_state(saved->xfer);
                    tag_left_ = saved->tag_left;
                    tag_flg_ = saved->tag_flg;
                    tag_nreg_ = saved->tag_nreg;
                    tag_reg_ = saved->tag_reg;
                    tag_regs_ = saved->tag_regs;
                    saved.reset();
                }
                if (in.regs_tag != 0 && !in_sub && !drop && !frame.regs_at_ok && addr == (in.regs_tag & 0x01fffff0u)) {  // (the registers before this tag)
                    std::memcpy(frame.regs_at, regs, sizeof regs);
                    frame.regs_at_ok = true;
                }
                sub_wide_ = in_sub;
                sub_drop_ = drop;
                ++frame.tags;
                const uint8_t sb = in_sub ? static_cast<uint8_t>(cur + 1) : 0;
                marks(addr, sb, in_sub);
                const bool dry = skip >= 0 && in.skips[static_cast<size_t>(skip)].dry;
                if (skip >= 0) {
                    if (dry) ++frame.skipped_tags;
                    const size_t k = static_cast<size_t>(skip);
                    if (!skip_started_[k]) {  // the range's VU1 program starts, once: the double buffer flipped
                        skip_started_[k] = true;
                        const MrpChainInput::Skip& r = in.skips[k];
                        if (r.moby && frame.moby_mark == SIZE_MAX) frame.moby_mark = frame.prims.size();
                        if (r.host_3d != 0) {  // the host's 3D in the primitives' order, with context 1's state now
                            if (scr_prim_ >= 0) screen_picture();
                            Primitive p;
                            p.type = 8;
                            DrawState& s = p.st;
                            s.prim = regs[0x00];
                            s.alpha = regs[0x42];
                            s.test = regs[0x47];
                            s.scissor = regs[0x40];
                            s.fba = regs[0x4a];
                            s.frame = regs[0x4c];
                            s.zbuf = regs[0x4e];
                            s.xyoffset = regs[0x18];
                            p.fx_id = r.host_3d;
                            frame.prims.push_back(std::move(p));
                        }
                        if (dry) {
                            for (uint32_t m = 0; m < r.mscals; ++m) vu1::flip_buffers(vu_, vu_.next_pc);
                            frame.flips += r.mscals;
                        }
                    }
                    if (dry) return;  // (its VIF codes and data not read)
                }
                // the tag's VIF codes (its upper 64 bits; its lower half, not sent to the VIF, as two NOPs), then its data from
                // the next quadword, one stream
                buf_.assign(16, 0);
                std::memcpy(buf_.data() + 8, t + 8, 8);
                if (d != nullptr) buf_.insert(buf_.end(), d, d + static_cast<size_t>(qwc) * 16);
                vif_stream(buf_.data(), static_cast<uint32_t>(buf_.size()));
            });
        if (scr_prim_ >= 0) screen_picture();
        if (saved != nullptr) mem.undo_end();  // (a chain that ended inside a substitute)
        in_ = nullptr;
    }

    // vu1::StartObserver (this thread's program starts): program 7's batch in a vu7 range kept as the program finds it
    void before(vu1::Vu1& vu, uint32_t start, uint64_t hash) override;
    void after(vu1::Vu1&, uint32_t, uint64_t) override { vu7_drop_ = false; }

private:
    const MrpChainInput* in_ = nullptr;
    vu1::Vu1 vu_;
    Vertex cur_;
    std::vector<Vertex> queue_;
    uint32_t tag_left_ = 0, tag_flg_ = 0, tag_nreg_ = 1, tag_reg_ = 0;
    uint64_t tag_regs_ = 0;
    bool vu_kick_ = false;
    std::vector<bool> skip_started_;
    bool sub_wide_ = false, sub_drop_ = false;
    uint8_t fx_now_ = 0;
    uint32_t fx_id_ = 0;
    bool layer_now_ = false;
    uint8_t hud_now_ = 0;
    bool vu7_now_ = false;
    uint32_t vu7_tag_ = 0;
    bool vu7_drop_ = false;
    bool kicks_dropped_ = false;  // a program outside the ranges runs: its kicks read for the GS registers only
    long scr_prim_ = -1;

    // the ranges the tag at `addr` is in (`sb`: the substitute's index + 1, 0 the real chain)
    void marks(uint32_t addr, uint8_t sb, bool in_sub) {
        const MrpChainInput& in = *in_;
        fx_now_ = 0;
        fx_id_ = 0;
        if (!in.fx.empty()) {
            auto it = std::upper_bound(in.fx.begin(), in.fx.end(), std::make_pair(sb, addr), [](const std::pair<uint8_t, uint32_t>& k, const MrpChainInput::Fx& r) {
                return k.first != r.sub ? k.first < r.sub : k.second < r.first;
            });
            if (it != in.fx.begin()) {
                --it;
                if (it->sub == sb && addr >= it->first && addr <= it->last) {
                    fx_now_ = it->kind;
                    fx_id_ = it->id;
                }
            }
        }
        if (fx_now_ != 0 && frame.fx_mark == SIZE_MAX) frame.fx_mark = frame.prims.size();
        layer_now_ = false;
        if (!in.layers.empty() && !in_sub) {
            auto it = std::upper_bound(in.layers.begin(), in.layers.end(), addr, [](uint32_t a, const std::pair<uint32_t, uint32_t>& r) { return a < r.first; });
            layer_now_ = it != in.layers.begin() && addr <= std::prev(it)->second;
        }
        vu7_now_ = false;
        if (!in.vu7.empty()) {
            auto it = std::upper_bound(in.vu7.begin(), in.vu7.end(), std::make_pair(sb, addr), [](const std::pair<uint8_t, uint32_t>& k, const MrpChainInput::Vu7& r) {
                return k.first != r.sub ? k.first < r.sub : k.second < r.first;
            });
            if (it != in.vu7.begin()) {
                --it;
                vu7_now_ = it->sub == sb && addr >= it->first && addr <= it->last;
                vu7_tag_ = it->tag;
            }
        }
        hud_now_ = 0;
        if (!in.hud.empty() && !in_sub) {
            auto it = std::upper_bound(in.hud.begin(), in.hud.end(), addr, [](uint32_t a, const MrpChainInput::Hud& r) { return a < r.first; });
            if (it != in.hud.begin() && addr <= std::prev(it)->last) hud_now_ = std::prev(it)->side;
        }
    }

    std::vector<uint8_t> buf_;
    void vif_stream(const uint8_t* p, uint32_t n);
    void gif(const uint8_t* p, uint32_t qwc);
    void packed(const uint8_t* q, uint32_t reg);
    void write_reg(uint32_t reg, uint64_t v);
    void vertex_kick(const Vertex& vin, bool draw);
    void screen_transfer(uint32_t dbp);
    void screen_picture();
    void mscal(uint32_t code);
};

// orig: (host)
void Reader::vif_stream(const uint8_t* p, uint32_t n) {
    uint32_t i = 0;
    while (i + 4 <= n) {
        const uint32_t code = r32(p + i);
        i += 4;
        const uint32_t cmd = (code >> 24) & 0x7f, imm = code & 0xffff;
        if (cmd == 0x50 || cmd == 0x51) {  // DIRECT / DIRECTHL: GIF data (path 2)
            const uint32_t q = imm == 0 ? 65536 : imm;
            i = (i + 15) & ~15u;
            const uint32_t take = std::min(q * 16, n - std::min(n, i));
            gif(p + i, take / 16);
            i += q * 16;
        } else {  // the VU1 side: cycle, masks, UNPACK, MPG, the double buffer, program starts
            const uint32_t nb = vu1::vif_data_bytes(vu_, code);
            if (nb == 0 || i + nb <= n) {
                if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) mscal(code);
                else vu1::vif1_command(vu_, code, p + i, nullptr);
            }
            i += nb;
        }
        // NOP, MSKPATH3, MARK, FLUSH*: nothing to do
    }
}

// A program start (MSCAL / MSCALF / MSCNT): run as the GS replica runs it (the host's hook first: the MRP's mobys' programs are not
// run, vu1::g_start_hook), so that the VU1's state - its registers, its clip flags, which the next program reads - is the same; its
// kicks kept only inside the effects' and program 7's ranges (not in a real range a substitute stood in for: its 3D is the
// substitute's), elsewhere read for the GS registers they set and dropped (the moby programs' at the boot's title, whose logo
// draws with the state their last kick left).
// orig: (host)
void Reader::mscal(uint32_t code) {
    const bool range = (fx_now_ != 0 || vu7_now_) && !sub_drop_;
    kicks_dropped_ = !range;
    ++frame.program_runs;
    vu1::vif1_command(vu_, code, nullptr, [this](const uint8_t* d, uint32_t q) {
        vu_kick_ = true;
        gif(d, q);
        vu_kick_ = false;
    });
    kicks_dropped_ = false;
}

// GIF data, a quadword at a time: a tag and its data may arrive in separate transfers (an IMAGE tag in a CNT, its pixels in the
// next REF), so the tag in progress is kept.
// orig: (host)
void Reader::gif(const uint8_t* p, uint32_t qwc) {
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
            gs::local_memory().transfer_data(regs[0x50], regs[0x51], regs[0x52], q, take * 16);
            tag_left_ -= take;
            i += take - 1;
        }
    }
}

// orig: (host)
void Reader::packed(const uint8_t* q, uint32_t d) {
    const uint32_t w0 = r32(q), w1 = r32(q + 4), w2 = r32(q + 8), w3 = r32(q + 12);
    switch (d) {
    case 0x0: write_reg(0x00, w0 & 0x7ff); break;
    case 0x1:
        cur_.r = w0 & 0xff; cur_.g = w1 & 0xff; cur_.b = w2 & 0xff; cur_.a = w3 & 0xff;
        regs[0x01] = (w0 & 0xff) | ((w1 & 0xff) << 8) | ((w2 & 0xff) << 16) | (static_cast<uint64_t>(w3 & 0xff) << 24);
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
    case 0xe: write_reg(byte_at(q, 8), r64(q)); break;
    case 0xf: break;
    default: write_reg(d, r64(q)); break;  // TEX0 / CLAMP: the register's own layout
    }
}

// orig: (host)
void Reader::write_reg(uint32_t reg, uint64_t v) {
    reg &= 0x7f;
    regs[reg] = v;
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
            gs::local_memory().transfer_start();
            if (in_->scr_bw != 0) screen_transfer(bits(regs[0x50], 32, 14));
        } else {
            gs::local_memory().transfer_stop();
        }
        break;
    default: break;
    }
}

// orig: (host)
void Reader::vertex_kick(const Vertex& vin, bool draw) {
    if (vu7_drop_ && vu_kick_ && !frame.vu7_batches.empty()) {  // (a kept batch's strip as the program kicked it: its ADC flags)
        const uint32_t t = bits(regs[0x00], 0, 3);
        if (t == 3 || t == 4) frame.vu7_batches.back().kicked.push_back(draw ? 1 : 0);
    }
    const uint64_t prim = bits(regs[0x1a], 0, 1) ? regs[0x00] : (regs[0x1b] & ~7ull) | (regs[0x00] & 7);
    const int ctx = bits(prim, 9, 1);
    const uint64_t off = regs[0x18 + ctx];
    Vertex v = vin;
    v.x = (v.x - static_cast<float>(bits(off, 0, 16))) / 16.0f;
    v.y = (v.y - static_cast<float>(bits(off, 32, 16))) / 16.0f;
    queue_.push_back(v);
    const int type = bits(regs[0x00], 0, 3);
    int need = 0, assembled = 0;
    switch (type) {
    case 0: need = 1; assembled = 0; break;
    case 1: case 2: need = 2; assembled = 1; break;
    case 3: case 4: case 5: need = 3; assembled = 3; break;
    case 6: need = 2; assembled = 6; break;
    default: queue_.clear(); return;
    }
    if (static_cast<int>(queue_.size()) < need) return;
    // kept: not a kick of program 7's kept batch (the MRP draws the batch from its input), not a kick of a program run only for
    // its GS state, not an effect of a real range a substitute stood in for (the substitute's are drawn)
    const bool keep = draw && type != 0 && !(vu_kick_ && (vu7_drop_ || kicks_dropped_)) && !(sub_drop_ && fx_now_ != 0);
    if (draw && type != 0 && vu_kick_ && kicks_dropped_) ++frame.dropped_3d;
    if (keep) {
        if (scr_prim_ >= 0) screen_picture();  // (the frame's picture put back before this primitive)
        Primitive p;
        p.type = assembled;
        DrawState& s = p.st;
        s.prim = prim;
        s.tex0 = regs[0x06 + ctx];
        s.clamp = regs[0x08 + ctx];
        s.tex1 = regs[0x14 + ctx];
        s.miptbp1 = regs[0x34 + ctx];
        s.alpha = regs[0x42 + ctx];
        s.test = regs[0x47 + ctx];
        s.scissor = regs[0x40 + ctx];
        s.fba = regs[0x4a + ctx];
        s.frame = regs[0x4c + ctx];
        s.zbuf = regs[0x4e + ctx];
        s.texa = regs[0x3b];
        s.colclamp = regs[0x46];
        s.pabe = regs[0x49];
        s.fogcol = regs[0x3d];
        s.dthe = regs[0x45];
        s.xyoffset = off;
        p.v.assign(queue_.end() - need, queue_.end());
        if (bits(prim, 4, 1) != 0) {
            gs::LocalMemory& mem = gs::local_memory();
            // mipmapping (TEX1 MXL > 0, MMIN *_MIPMAP_*): the levels the primitive's vertices span (LCM 0: log2(1 / |Q|) << L + K;
            // LCM 1: K), rounded, clamped to MXL; level n's base / width from MIPTBP1, its size halved n times
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
                    const int idx = l - lo - 1;
                    if (idx >= 0 && idx < 3) {
                        p.mip_tex0[idx] = level_tex0(l);
                        p.mip_tex[idx] = mem.texture_at_kick(p.mip_tex0[idx], s.texa);
                    }
                }
                s.tex0 = level_tex0(lo);
            }
            p.tex = mem.texture_at_kick(s.tex0, s.texa);
        }
        if (!bits(prim, 3, 1))  // flat: the last vertex's colour
            for (Vertex& w : p.v) { w.r = p.v.back().r; w.g = p.v.back().g; w.b = p.v.back().b; w.a = p.v.back().a; }
        p.vu = vu_kick_;
        p.vu_start = vu_kick_ ? vu1::g_last_start : 0;
        p.sub = sub_wide_ ? 1 : sub_drop_ ? 2 : 0;
        p.wide = sub_wide_;
        p.fx = fx_now_;
        p.fx_id = fx_id_;
        p.layer = layer_now_;
        p.hud = hud_now_;
        frame.prims.push_back(std::move(p));
    }
    switch (type) {
    case 2: queue_.erase(queue_.begin(), queue_.end() - 1); break;
    case 4: queue_.erase(queue_.begin()); break;
    case 5: queue_.erase(queue_.begin() + 1); break;
    default: queue_.clear(); break;
    }
}

// an IMAGE transfer into the frame (MrpChainInput::scr_*): a type-7 primitive, its picture read once the transfers in a row are done
// orig: (host)
void Reader::screen_transfer(uint32_t dbp) {
    const MrpChainInput& in = *in_;
    const uint32_t blocks = in.scr_bw * 64 * in.scr_h * 4 / 256;
    if (dbp < in.scr_bp || dbp >= in.scr_bp + blocks || scr_prim_ >= 0) return;
    Primitive p;
    p.type = 7;
    p.v.resize(2);
    p.v[1].x = static_cast<float>(in.scr_bw * 64);
    p.v[1].y = static_cast<float>(in.scr_h);
    p.st.frame = (in.scr_bp / 32) | (static_cast<uint64_t>(in.scr_bw) << 16);
    scr_prim_ = static_cast<long>(frame.prims.size());
    frame.prims.push_back(std::move(p));
}

// orig: (host)
void Reader::screen_picture() {
    const MrpChainInput& in = *in_;
    frame.prims[static_cast<size_t>(scr_prim_)].tex = gs::local_memory().picture_ct32(in.scr_bp, in.scr_bw, in.scr_bw * 64, in.scr_h);
    scr_prim_ = -1;
}

// Before a program start on the reader's VU1: a batch of program 7 in a vu7 range kept as the program finds it (its GIF tag, then
// per vertex ST, RGBA, XYZ at XTOP; the constants the start-up loaded: vf18..vf21, vf30, vf31.w, vf7), with the GS state its kick
// would have, and its place in the primitives' order (a type-9 primitive)
// orig: (host)
void Reader::before(vu1::Vu1& vu, uint32_t start, uint64_t hash) {
    const MrpChainInput& in = *in_;
    if (!vu7_now_ || sub_drop_ || hash != in.vu7_hash || in.vu7_kind == 0 || (start != 0xc && start != 0xe)) return;
    const VUState& st = vu.st;
    auto q = [&st](uint32_t a) { return &st.mem[(a & 0x3ffu) << 4]; };
    const uint32_t x = st.xtop;
    const uint64_t tag = r64(q(x));
    const uint32_t n = static_cast<uint32_t>(tag & 0x7fff);
    if (n < 3 || n > 0x150) return;  // (a batch holds at most (0x1d2 - 1) / 3 vertices)
    Vu7Batch b;
    // the GS state a kick under a GIF tag `t` would have now (its PRIM when PRE, the context's registers)
    auto state_of = [this](uint64_t t, DrawState& s, std::shared_ptr<const TexImage>& tex, bool* mip) {
        const uint64_t prim0 = bits(t, 46, 1) != 0 ? bits(t, 47, 11) : regs[0x00];
        const uint64_t prim = bits(regs[0x1a], 0, 1) ? prim0 : (regs[0x1b] & ~7ull) | (prim0 & 7);
        const int ctx = bits(prim, 9, 1);
        s.prim = prim;
        s.tex0 = regs[0x06 + ctx];
        s.clamp = regs[0x08 + ctx];
        s.tex1 = regs[0x14 + ctx];
        s.miptbp1 = regs[0x34 + ctx];
        s.alpha = regs[0x42 + ctx];
        s.test = regs[0x47 + ctx];
        s.scissor = regs[0x40 + ctx];
        s.fba = regs[0x4a + ctx];
        s.frame = regs[0x4c + ctx];
        s.zbuf = regs[0x4e + ctx];
        s.texa = regs[0x3b];
        s.colclamp = regs[0x46];
        s.pabe = regs[0x49];
        s.fogcol = regs[0x3d];
        s.dthe = regs[0x45];
        s.xyoffset = regs[0x18 + ctx];
        if (bits(prim, 4, 1) != 0) {
            tex = gs::local_memory().texture_at_kick(s.tex0, s.texa);
            if (mip != nullptr) *mip = std::min(bits(s.tex1, 2, 3), 3u) != 0 && bits(s.tex1, 6, 3) >= 2;
        }
    };
    state_of(tag, b.st, b.tex, &b.mip);
    if (start == 0xe) {
        const uint64_t ft = r64(reinterpret_cast<const uint8_t*>(&st.vf[31].u[0]));
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
    b.first = static_cast<uint32_t>(frame.vu7_verts.size());
    b.n = n;
    b.start = start;
    b.tag = vu7_tag_;
    b.sub = sub_wide_ ? 1 : 0;
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
        frame.vu7_verts.push_back(v);
    }
    if (scr_prim_ >= 0) screen_picture();
    Primitive p;  // (its place in the primitives' order)
    p.type = 9;
    p.st = s;
    p.fx = in.vu7_kind;
    p.fx_id = static_cast<uint32_t>(frame.vu7_batches.size());
    p.sub = b.sub;
    p.wide = sub_wide_;
    p.vu = true;
    p.vu_start = start;
    frame.prims.push_back(std::move(p));
    frame.vu7_batches.push_back(std::move(b));
    vu7_drop_ = true;
}

// The capture: the walk's tags and data copied in its order (the game's thread)
// orig: (host)
void capture(MrpChainInput&& in, Capture& c) {
    c.in = std::move(in);
    std::sort(c.in.skips.begin(), c.in.skips.end(), [](const MrpChainInput::Skip& a, const MrpChainInput::Skip& b) { return a.first < b.first; });
    std::sort(c.in.fx.begin(), c.in.fx.end(), [](const MrpChainInput::Fx& a, const MrpChainInput::Fx& b) { return a.sub != b.sub ? a.sub < b.sub : a.first < b.first; });
    std::sort(c.in.vu7.begin(), c.in.vu7.end(), [](const MrpChainInput::Vu7& a, const MrpChainInput::Vu7& b) { return a.sub != b.sub ? a.sub < b.sub : a.first < b.first; });
    std::sort(c.in.layers.begin(), c.in.layers.end());
    std::sort(c.in.hud.begin(), c.in.hud.end(), [](const MrpChainInput::Hud& a, const MrpChainInput::Hud& b) { return a.first < b.first; });
    for (auto& u : c.in.subs) {
        u.first &= 0x01fffff0u;
        u.end &= 0x01fffff0u;
    }
    c.in.subs.erase(std::remove_if(c.in.subs.begin(), c.in.subs.end(), [](const MrpChainInput::Sub& u) { return u.bytes.empty() || u.first == 0; }), c.in.subs.end());
    if (c.in.subs.size() > static_cast<size_t>(kMaxSubs)) c.in.subs.resize(kMaxSubs);
    c.bytes.clear();
    c.spans.clear();
    Walk w(c.in);
    w.run(
        [&c, &w](uint32_t a, uint32_t len) -> const uint8_t* {
            const uint8_t* src = w.sub_mem(a);
            const size_t at = c.bytes.size();
            c.spans.push_back({a, static_cast<uint32_t>(at), src == nullptr ? 0xffffffffu : len});
            if (src == nullptr) return nullptr;
            c.bytes.insert(c.bytes.end(), src, src + len);
            return c.bytes.data() + at;  // (valid until the next take: the walk reads the tag's words before asking for its data)
        },
        [](uint32_t, const uint8_t*, const uint8_t*, uint32_t, long, bool, bool, int, int) {});
}

// the reading on a worker thread of its own
struct Worker {
    std::mutex m;
    std::condition_variable cv;
    std::thread th;
    bool job = false, busy = false, quit = false;
    std::exception_ptr error;
#if defined(__WIIU__)
    double fpscr = 0.0;  // (the game's thread's float rules, mffs at the start: the VU1 programs read on the worker run under them, as job_pool.cpp's FloatEnv)
#endif
    void loop();
    ~Worker() {
        {
            std::lock_guard<std::mutex> lk(m);
            quit = true;
        }
        cv.notify_all();
        if (th.joinable()) th.join();
    }
};

Reader g_reader;
Capture g_capture;
Worker g_worker;
bool g_used = false;

// orig: (host)
void Worker::loop() {
    std::unique_lock<std::mutex> lk(m);
    for (;;) {
        cv.wait(lk, [this] { return job || quit; });
        if (quit) return;
        job = false;
#if defined(__WIIU__)
        __asm__ volatile("mtfsf 0xff, %0" : : "f"(fpscr));
#endif
        lk.unlock();
        try {
            const rac1::prof::Job prof_job(rac1::prof::kWorkerChain);  // (the debug menu's profiler: the job's time)
            g_reader.read(g_capture);
        } catch (...) {
            error = std::current_exception();
        }
        lk.lock();
        busy = false;
        cv.notify_all();
    }
}

}  // namespace

// orig: (host) see mrp_chain.h
void mrp_chain_start(MrpChainInput&& in, bool async) {
    mrp_chain_join();
    gs::add_memory_join(&mrp_chain_join);
    capture(std::move(in), g_capture);
    g_used = true;
    if (!async) {
        g_reader.read(g_capture);
        return;
    }
    Worker& w = g_worker;
    std::lock_guard<std::mutex> lk(w.m);
    if (!w.th.joinable()) w.th = std::thread([&w] {
        rac1::prof::thread_name("MRP chain worker");
#if defined(__WIIU__)
        // (the Wii U: the worker on a core of its own - RAC1_WIIU_CHAIN_CORE, default 0; the game runs on core 1, the IOP's mirror on
        // core 2 -, -1: wut's "any core")
        const char* ce = std::getenv("RAC1_WIIU_CHAIN_CORE");
        const int core = ce != nullptr ? std::atoi(ce) : 0;
        if (core >= 0 && core <= 2) OSSetThreadAffinity(OSGetCurrentThread(), static_cast<OSThreadAttributes>(OS_THREAD_ATTRIB_AFFINITY_CPU0 << core));
        std::printf("[mrp-chain] the chain worker on core %u (RAC1_WIIU_CHAIN_CORE %d)\n", OSGetCoreId(), core);
#endif
        w.loop();
    });
#if defined(__WIIU__)
    __asm__ volatile("mffs %0" : "=f"(w.fpscr));
#endif
    w.error = nullptr;
    w.busy = true;
    w.job = true;
    w.cv.notify_all();
}

// orig: (host) see mrp_chain.h
void mrp_chain_join() {
    RAC1_PROF_ZONE(kChainJoinWait);
    Worker& w = g_worker;
    std::unique_lock<std::mutex> lk(w.m);
    w.cv.wait(lk, [&w] { return !w.busy; });
    if (w.error) {
        std::exception_ptr e = w.error;
        w.error = nullptr;
        std::rethrow_exception(e);
    }
}

// orig: (host) see mrp_chain.h
const MrpChainFrame& mrp_chain_frame() { return g_reader.frame; }

// orig: (host) see mrp_chain.h
void mrp_chain_clear() {
    mrp_chain_join();
    g_reader.frame.prims.clear();
    g_reader.frame.vu7_batches.clear();
    g_reader.frame.vu7_verts.clear();
    g_reader.frame.fx_mark = g_reader.frame.moby_mark = SIZE_MAX;
    g_reader.frame.fx_external = false;
    g_reader.frame.regs_at_ok = false;
}

// orig: (host) see mrp_chain.h
bool mrp_chain_used() { return g_used; }
// orig: (host) see mrp_chain.h
void mrp_chain_set_used(bool used) { g_used = used; }

// orig: (host) see mrp_chain.h
uint64_t mrp_chain_gs_reg(uint32_t r) { return g_used ? g_reader.regs[r & 0x7f] : gs::gs_global().reg(r); }

// orig: (host) see mrp_chain.h
bool mrp_chain_wanted() {
    static const bool env_off = [] {
        const char* e = std::getenv("RAC1_MRP_CHAIN");
        return e != nullptr && std::atoi(e) == 0;
    }();
    return !env_off && g_moby_renderer == MobyRenderer::kMrp && g_world_mrp && g_mrp_fx && g_mrp_seas;
}

namespace {
uint64_t g_chk_frames = 0, g_chk_bad = 0;
}  // namespace

// orig: (host) see mrp_chain.h
void mrp_chain_check(uint64_t fr, const std::vector<gs::Primitive>& prims, const std::vector<gs::Vu7Batch>& batches,
                     const std::vector<gs::Vu7Vertex>& verts, size_t fx_mark, size_t moby_mark) {
    const MrpChainFrame& f = g_reader.frame;
    // the replica's primitives the MRP draws (the reader keeps only those), and where the marks fall among them
    std::vector<size_t> keep;
    size_t rfx = SIZE_MAX, rmoby = SIZE_MAX;
    for (size_t i = 0; i < prims.size(); ++i) {
        if (i == fx_mark && rfx == SIZE_MAX) rfx = keep.size();
        if (i == moby_mark && rmoby == SIZE_MAX) rmoby = keep.size();
        const gs::Primitive& p = prims[i];
        if (p.fx != 0 && p.fx_id == gs::Gs::kVu7Kicked) continue;
        if (p.fx != 0 && p.sub == 2) continue;
        if (p.moby) continue;
        keep.push_back(i);
    }
    if (fx_mark == prims.size()) rfx = keep.size();
    if (moby_mark == prims.size()) rmoby = keep.size();
    ++g_chk_frames;
    int shown = 0;
    auto bad = [&](const char* what, size_t i) {
        ++g_chk_bad;
        if (shown++ < 4) std::printf("[chain-check] frame %llu: %s at %zu (replica %zu prims kept of %zu, reader %zu)\n", static_cast<unsigned long long>(fr), what, i, keep.size(),
                                     prims.size(), f.prims.size());
    };
    if (keep.size() != f.prims.size()) bad("primitive count", 0);
    if (rfx != f.fx_mark && !(rfx == SIZE_MAX && f.fx_mark == f.prims.size())) bad("fx mark", rfx);
    if (rmoby != f.moby_mark && !(rmoby == SIZE_MAX && f.moby_mark == f.prims.size())) bad("moby mark", rmoby);
    const size_t n = std::min(keep.size(), f.prims.size());
    for (size_t k = 0; k < n; ++k) {
        const gs::Primitive& a = prims[keep[k]];
        const gs::Primitive& b = f.prims[k];
        // (a type-9 primitive's id is its batch's index: the replica's count the batches of the real ranges substitutes stood in for,
        // the reader keeps none of those - the batches themselves are compared below)
        const bool id_ok = a.type == 9 ? (a.fx_id < batches.size() && b.fx_id < f.vu7_batches.size() && batches[a.fx_id].n == f.vu7_batches[b.fx_id].n &&
                                          batches[a.fx_id].first < verts.size() && std::memcmp(&verts[batches[a.fx_id].first], &f.vu7_verts[f.vu7_batches[b.fx_id].first], sizeof(gs::Vu7Vertex)) == 0)
                                       : a.fx_id == b.fx_id;
        if (a.type != b.type || a.fx != b.fx || !id_ok || a.sub != b.sub || a.wide != b.wide || a.layer != b.layer || a.hud != b.hud || a.vu != b.vu) {
            if (shown < 4)
                std::printf("[chain-check]   marks replica type %d fx %u id %u sub %u wide %d layer %d hud %u vu %d | reader type %d fx %u id %u sub %u wide %d layer %d hud %u vu %d\n", a.type, a.fx, a.fx_id, a.sub,
                            a.wide, a.layer, a.hud, a.vu, b.type, b.fx, b.fx_id, b.sub, b.wide, b.layer, b.hud, b.vu);
            bad("primitive marks", k);
            continue;
        }
        if (std::memcmp(&a.st, &b.st, sizeof a.st) != 0) { bad("primitive state", k); continue; }
        if (a.v.size() != b.v.size()) { bad("vertex count", k); continue; }
        bool vd = false;
        for (size_t j = 0; j < a.v.size(); ++j) {
            const gs::Vertex& x = a.v[j];
            const gs::Vertex& y = b.v[j];
            auto fd = [](float p, float q) { return std::memcmp(&p, &q, sizeof p) != 0; };  // (bit for bit: a NaN Q is a NaN on both)
            vd = vd || fd(x.x, y.x) || fd(x.y, y.y) || x.z != y.z || x.r != y.r || x.g != y.g || x.b != y.b || x.a != y.a || fd(x.q, y.q) || fd(x.s, y.s) || fd(x.t, y.t) ||
                 fd(x.u, y.u) || fd(x.v, y.v) || x.fog != y.fog;
        }
        if (vd) {
            if (shown < 4)
                for (size_t j = 0; j < a.v.size(); ++j) {
                    const gs::Vertex& x = a.v[j];
                    const gs::Vertex& y = b.v[j];
                    std::printf("[chain-check]   v%zu replica xy %.2f %.2f z %u rgba %u %u %u %u q %g st %g %g uv %g %g f %u | reader xy %.2f %.2f z %u rgba %u %u %u %u q %g st %g %g uv %g %g f %u\n", j,
                                x.x, x.y, x.z, x.r, x.g, x.b, x.a, x.q, x.s, x.t, x.u, x.v, x.fog, y.x, y.y, y.z, y.r, y.g, y.b, y.a, y.q, y.s, y.t, y.u, y.v, y.fog);
                }
            bad("vertices", k);
            continue;
        }
        if ((a.tex == nullptr) != (b.tex == nullptr) || (a.tex != nullptr && (a.tex->w != b.tex->w || a.tex->h != b.tex->h || a.tex->px != b.tex->px))) bad("texture", k);
        if (a.mip_n != b.mip_n || a.mip_lo != b.mip_lo) bad("mip", k);
    }
    // program 7's batches: the replica's of the real chain and the substitutes (its sub-2 ones are never drawn)
    std::vector<size_t> rb;
    for (size_t i = 0; i < batches.size(); ++i)
        if (batches[i].sub != 2) rb.push_back(i);
    if (rb.size() != f.vu7_batches.size()) bad("vu7 batch count", rb.size());
    for (size_t k = 0; k < std::min(rb.size(), f.vu7_batches.size()); ++k) {
        const gs::Vu7Batch& a = batches[rb[k]];
        const gs::Vu7Batch& b = f.vu7_batches[k];
        bool d = a.n != b.n || a.start != b.start || a.tag != b.tag || a.sub != b.sub || a.mip != b.mip || a.kicked != b.kicked || std::memcmp(&a.st, &b.st, sizeof a.st) != 0 ||
                 std::memcmp(&a.st_fan, &b.st_fan, sizeof a.st_fan) != 0 || std::memcmp(a.m, b.m, sizeof a.m) != 0 || std::memcmp(a.off, b.off, sizeof a.off) != 0 ||
                 a.k != b.k || std::memcmp(a.fog, b.fog, sizeof a.fog) != 0 || std::memcmp(a.clip, b.clip, sizeof a.clip) != 0;
        for (uint32_t j = 0; !d && j < a.n; ++j)
            d = std::memcmp(&verts[a.first + j], &f.vu7_verts[b.first + j], sizeof(gs::Vu7Vertex)) != 0;
        if (!d && ((a.tex == nullptr) != (b.tex == nullptr) || (a.tex != nullptr && a.tex->px != b.tex->px))) d = true;
        if (d && shown < 4)
            std::printf("[chain-check]   batch n %u/%u start %x/%x tag %x/%x sub %u/%u mip %d/%d kicked %zu/%zu eq %d st %d fan %d m %d off %d k %d fog %d clip %d tex %d/%d\n", a.n, b.n, a.start, b.start, a.tag,
                        b.tag, a.sub, b.sub, a.mip, b.mip, a.kicked.size(), b.kicked.size(), a.kicked == b.kicked, std::memcmp(&a.st, &b.st, sizeof a.st) == 0,
                        std::memcmp(&a.st_fan, &b.st_fan, sizeof a.st_fan) == 0, std::memcmp(a.m, b.m, sizeof a.m) == 0, std::memcmp(a.off, b.off, sizeof a.off) == 0, a.k == b.k,
                        std::memcmp(a.fog, b.fog, sizeof a.fog) == 0, std::memcmp(a.clip, b.clip, sizeof a.clip) == 0, a.tex != nullptr, b.tex != nullptr);
        if (d && shown < 2 && a.kicked != b.kicked) {
            std::printf("[chain-check]   kicked replica");
            for (uint8_t x : a.kicked) std::printf(" %u", x);
            std::printf(" | reader");
            for (uint8_t x : b.kicked) std::printf(" %u", x);
            std::printf("\n");
        }
        if (d) bad("vu7 batch", k);
    }
    if (g_chk_frames % 300 == 0 || shown > 0)
        std::printf("[chain-check] %llu frames compared, %llu differences (frame %llu: reader %zu prims, %llu tags, %llu skipped, %llu program runs, %llu flips)\n",
                    static_cast<unsigned long long>(g_chk_frames), static_cast<unsigned long long>(g_chk_bad), static_cast<unsigned long long>(fr), f.prims.size(),
                    static_cast<unsigned long long>(f.tags), static_cast<unsigned long long>(f.skipped_tags), static_cast<unsigned long long>(f.program_runs),
                    static_cast<unsigned long long>(f.flips));
}

}  // namespace rac1
