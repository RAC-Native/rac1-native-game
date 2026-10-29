// VU1 behind VIF1 (see vu1.h). Hardware layer: no game logic; the programs are the game's own
// (uploaded by MPG from its packets) or their registered ports.
#include <runtime/vu/vu1.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace rac1::vu1 {

namespace {

struct Registered {
    uint32_t addr, n;
    uint64_t hash;
    ProgramFn fn;
    const char* name;
};
std::vector<Registered>& registry() {
    static std::vector<Registered> r;
    return r;
}

uint32_t r32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

uint32_t* mem_word(VUState& st, uint32_t qw, uint32_t lane) {
    return reinterpret_cast<uint32_t*>(&st.mem[((qw & 0x3ff) << 4) + (lane << 2)]);
}

// UNPACK without mask, row mode or filling, one format: the same words as unpack()'s general loop
template <uint32_t VN, uint32_t EBYTES, bool USN>
void unpack_plain_t(Vu1& vu, uint32_t addr, uint32_t num, uint32_t wl, uint32_t cl, const uint8_t* d) {
    uint32_t pos = 0, row = addr;  // (dest = addr + (n / wl) * cl + n % wl, without the divisions)
    for (uint32_t n = 0; n < num; ++n) {
        const uint32_t dest = row + pos;
        if (++pos == wl) {
            pos = 0;
            row += cl;
        }
        uint8_t* out = &vu.st.mem[(dest & 0x3ff) << 4];
        const size_t at = static_cast<size_t>(n) * (VN + 1) * EBYTES;  // (from d: d is EE memory, word-aligned)
        uint32_t e[4];
        for (uint32_t k = 0; k <= VN; ++k) {
            if constexpr (EBYTES == 4) {
                std::memcpy(&e[k], d + at + 4 * k, 4);
            } else if constexpr (EBYTES == 2) {
                uint16_t h;
                std::memcpy(&h, d + ee_half_addr(static_cast<uint32_t>(at + 2 * k)), 2);
                e[k] = USN ? h : static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(h)));
            } else {
                const uint8_t b = d[ee_byte_addr(static_cast<uint32_t>(at + k))];
                e[k] = USN ? b : static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(b)));
            }
        }
        uint32_t val[4];
        if constexpr (VN == 0) val[0] = val[1] = val[2] = val[3] = e[0];
        else if constexpr (VN == 1) { val[0] = e[0]; val[1] = e[1]; val[2] = e[0]; val[3] = e[1]; }  // V2: (x, y, x, y)
        else if constexpr (VN == 2) { val[0] = e[0]; val[1] = e[1]; val[2] = e[2]; val[3] = e[0]; }  // V3: w (undefined) x
        else { val[0] = e[0]; val[1] = e[1]; val[2] = e[2]; val[3] = e[3]; }
        std::memcpy(out, val, 16);
    }
}
template <uint32_t VN, uint32_t EBYTES>
void unpack_plain_s(Vu1& vu, bool usn, uint32_t addr, uint32_t num, uint32_t wl, uint32_t cl, const uint8_t* d) {
    if (usn) unpack_plain_t<VN, EBYTES, true>(vu, addr, num, wl, cl, d);
    else unpack_plain_t<VN, EBYTES, false>(vu, addr, num, wl, cl, d);
}
template <uint32_t VN>
void unpack_plain_v(Vu1& vu, uint32_t vl, bool usn, uint32_t addr, uint32_t num, uint32_t wl, uint32_t cl, const uint8_t* d) {
    if (vl == 0) unpack_plain_s<VN, 4>(vu, usn, addr, num, wl, cl, d);
    else if (vl == 1) unpack_plain_s<VN, 2>(vu, usn, addr, num, wl, cl, d);
    else unpack_plain_s<VN, 1>(vu, usn, addr, num, wl, cl, d);
}
void unpack_plain(Vu1& vu, uint32_t vn, uint32_t vl, bool usn, uint32_t addr, uint32_t num, uint32_t wl, uint32_t cl, const uint8_t* d) {
    switch (vn) {
    case 0: unpack_plain_v<0>(vu, vl, usn, addr, num, wl, cl, d); break;
    case 1: unpack_plain_v<1>(vu, vl, usn, addr, num, wl, cl, d); break;
    case 2: unpack_plain_v<2>(vu, vl, usn, addr, num, wl, cl, d); break;
    default: unpack_plain_v<3>(vu, vl, usn, addr, num, wl, cl, d); break;
    }
}

// UNPACK: `num` vectors (0 = 256) of the format vn / vl into data memory from `imm`'s address
// (+ TOPS with FLG), with the write cycle (CL / WL: skipping or filling), the mode (offset /
// difference with the row) and, with the M bit, the mask (data / row / column / protected).
void unpack(Vu1& vu, uint32_t code, const uint8_t* d) {
    const uint32_t cmd = (code >> 24) & 0x7f, imm = code & 0xffff;
    const uint32_t num = ((code >> 16) & 0xff) == 0 ? 256 : (code >> 16) & 0xff;
    const uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3;
    const bool masked = (cmd & 0x10) != 0, usn = (imm & 0x4000) != 0;
    uint32_t addr = imm & 0x3ff;
    if ((imm & 0x8000) != 0) addr += vu.tops;
    const bool v45 = vn == 3 && vl == 3;
    const uint32_t ebits = 32u >> vl;
    uint64_t bitpos = 0;
    auto element = [&]() -> uint32_t {
        const uint32_t byte = static_cast<uint32_t>(bitpos >> 3);
        uint32_t v = 0;
        if (ebits == 32) v = r32(d + byte);
        else if (ebits == 16) {
            uint16_t h;
            std::memcpy(&h, d + ee_half_addr(byte), 2);  // (d is EE memory: a halfword's, a byte's place in its word)
            v = usn ? h : static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(h)));
        } else {
            const uint8_t b = d[ee_byte_addr(byte)];
            v = usn ? b : static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(b)));
        }
        bitpos += ebits;
        return v;
    };
    const uint32_t cl = vu.cl, wl = vu.wl == 0 ? 1 : vu.wl;
    // the common case: no mask, no row mode, no filling (every vector has data and is written whole): the same
    // words as the general loop below, without its per-lane decisions, one loop per format
    if (!masked && vu.mode == 0 && wl <= cl && vl != 3) {
        unpack_plain(vu, vn, vl, usn, addr, num, wl, cl, d);
        return;
    }
    for (uint32_t n = 0; n < num; ++n) {
        const uint32_t pos = n % wl;
        const uint32_t dest = wl <= cl ? addr + (n / wl) * cl + pos : addr + n;
        const bool has_data = wl <= cl || pos < cl;
        uint32_t val[4] = {0, 0, 0, 0};
        if (has_data) {
            if (v45) {
                uint16_t h;
                std::memcpy(&h, d + ee_half_addr(static_cast<uint32_t>(bitpos >> 3)), 2);
                bitpos += 16;
                val[0] = (h & 0x1fu) << 3;
                val[1] = ((h >> 5) & 0x1fu) << 3;
                val[2] = ((h >> 10) & 0x1fu) << 3;
                val[3] = (h >> 15) << 7;
            } else {
                uint32_t e[4] = {0, 0, 0, 0};
                for (uint32_t k = 0; k <= vn; ++k) e[k] = element();
                // S: x everywhere; V2: (x, y, x, y); V3: w undefined in hardware (here x)
                if (vn == 0) val[0] = val[1] = val[2] = val[3] = e[0];
                else if (vn == 1) { val[0] = e[0]; val[1] = e[1]; val[2] = e[0]; val[3] = e[1]; }
                else if (vn == 2) { val[0] = e[0]; val[1] = e[1]; val[2] = e[2]; val[3] = e[0]; }
                else for (int k = 0; k < 4; ++k) val[k] = e[k];
            }
        }
        const uint32_t cyc = std::min(pos, 3u);
        for (uint32_t c = 0; c < 4; ++c) {
            uint32_t mk = masked ? (vu.mask >> (cyc * 8 + c * 2)) & 3 : 0;
            if (!has_data && mk == 0) mk = 1;  // filling without data: the row
            uint32_t v;
            if (mk == 0) {
                v = val[c];
                if (vu.mode == 1) v += vu.row[c];
                else if (vu.mode == 2) v = vu.row[c] += val[c];
            } else if (mk == 1) v = vu.row[c];
            else if (mk == 2) v = vu.col[cyc];
            else continue;
            *mem_word(vu.st, dest, c) = v;
        }
    }
}

// Testing (RAC1_VU1_CAPTURE=<dir>): the state a program starts on (registers, data and micro
// memory), the first few starts of each address, for the oracle's proof of the ported programs.
void capture(const Vu1& vu, uint32_t start, const char* dir) {
    static std::map<uint32_t, int> count;
    static int total = 0;
    // RAC1_VU1_CAPTURE_ALL=<n>: every program image (the whole micro memory's hash), n states per start of each, spread over the calls
    // (the world's tfrag / tie / shrub programs)
    static const char* const all_env = std::getenv("RAC1_VU1_CAPTURE_ALL");
    char path[512];
    if (all_env != nullptr) {
        static std::map<std::pair<uint64_t, uint32_t>, std::pair<int, int>> seen;  // (image, start) -> (calls, saved)
        const int per = std::max(1, std::atoi(all_env));
        if (start >= 0x800 || total >= 4000) return;  // (garbage starts of the loading screen)
        auto& e = seen[{code_hash(vu.micro.data(), static_cast<uint32_t>(vu.micro.size())), start}];
        if (e.second >= per || (e.first++ % 7) != 0) return;
        ++e.second;
        std::snprintf(path, sizeof path, "%s/vu1w_%016llx_%03x_%03d.bin", dir,
                      static_cast<unsigned long long>(code_hash(vu.micro.data(), static_cast<uint32_t>(vu.micro.size()))), start, total++);
    } else {
        if (start >= 0x508) return;  // (the moby programs only: the loading screen starts garbage addresses)
        if (total >= 600 || count[start] >= 40) return;
        static int frame_skip = 0;
        if (++frame_skip % 13 != 0) return;  // (spread over the frames)
        ++count[start];
        std::snprintf(path, sizeof path, "%s/vu1_%03x_%03d.bin", dir, start, total++);
    }
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) return;
    const VUState& st = vu.st;
    const uint32_t hdr[4] = {0x31555676u, start, st.xtop, 0};
    std::fwrite(hdr, 4, 4, f);
    std::fwrite(st.vf.data(), sizeof(VUVector), 32, f);
    std::fwrite(st.vi.data(), 2, 16, f);
    std::fwrite(&st.acc, sizeof(VUVector), 1, f);
    const float fr[4] = {st.q, st.i, st.r, st.p};
    std::fwrite(fr, 4, 4, f);
    const uint32_t fl[4] = {st.macflag, st.statusflag, st.clipflag, 0};
    std::fwrite(fl, 4, 4, f);
    std::fwrite(st.mem.data(), 1, st.mem.size(), f);
    std::fwrite(vu.micro.data(), 8, vu.micro.size(), f);
    std::fclose(f);
}

}  // namespace

bool g_force_interpreter = false;
thread_local uint32_t g_last_start = 0;
thread_local float g_side_widen[2] = {1.0f, 1.0f};
thread_local StartObserver* g_start_observer = nullptr;
KickSink g_kick_tap;
StartHook g_start_hook;

namespace {
// (atomic, and the start memo below per thread: the MRP world's scene walker runs its private VU1 on a worker thread
// while the host runs the replica's)
std::atomic<uint64_t> g_micro_gen{1};  // counts the writes of the micro memory and the registered ports: the hash check of a program start holds until it changes
bool trace_on() {
    static const bool on = std::getenv("RAC1_VU1_TRACE") != nullptr;
    return on;
}
// testing (RAC1_VU_FALLBACK): (start, micro image hash) -> (starts, ns); the replica's VU1 and the MRP walker's private one
// run on two threads
std::map<std::pair<uint32_t, uint64_t>, std::pair<uint64_t, uint64_t>>& vu1_fallbacks() {
    static std::map<std::pair<uint32_t, uint64_t>, std::pair<uint64_t, uint64_t>> m;
    return m;
}
std::mutex g_fb_mutex;
}  // namespace

void fallback_report() {
    if (std::getenv("RAC1_VU_FALLBACK") == nullptr) return;
    const bool interp = std::getenv("RAC1_VU1_INTERP") != nullptr;
    std::lock_guard<std::mutex> lock(g_fb_mutex);
    for (const auto& kv : vu1_fallbacks())
        std::fprintf(stderr, "[vu-fallback] vu1 start %03x micro %016llx: %llu starts %s, %.1f ms\n", kv.first.first, static_cast<unsigned long long>(kv.first.second),
                     static_cast<unsigned long long>(kv.second.first), interp ? "on the interpreter" : "skipped (no port)", static_cast<double>(kv.second.second) / 1e6);
    if (vu1_fallbacks().empty()) std::fprintf(stderr, "[vu-fallback] vu1: every program ran as a port\n");
}

Vu1& vu1_global() {
    static Vu1 vu = [] {
        Vu1 v;
        v.st.vf[0].f[3] = 1.0f;  // vf0 = (0, 0, 0, 1)
        return v;
    }();
    return vu;
}

uint64_t code_hash(const uint64_t* words, uint32_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < n; ++i)
        for (int b = 0; b < 8; ++b) {
            h ^= (words[i] >> (8 * b)) & 0xff;
            h *= 0x100000001b3ull;
        }
    return h;
}

void register_program(uint32_t addr, uint32_t n, uint64_t hash, ProgramFn fn, const char* name) {
    registry().push_back({addr, n, hash, fn, name});
    ++g_micro_gen;
}

uint32_t vif_data_bytes(const Vu1& vu, uint32_t code) {
    const uint32_t cmd = (code >> 24) & 0x7f, num = (code >> 16) & 0xff;
    if (cmd == 0x20) return 4;                       // STMASK
    if (cmd == 0x30 || cmd == 0x31) return 16;       // STROW / STCOL
    if (cmd == 0x4a) return (num == 0 ? 256 : num) * 8;  // MPG
    if (cmd >= 0x60) {                               // UNPACK
        const uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3;
        uint32_t cnt = num == 0 ? 256 : num;
        const uint32_t wl = vu.wl == 0 ? 1 : vu.wl;
        if (wl > vu.cl) cnt = vu.cl * (cnt / wl) + std::min(cnt % wl, vu.cl);
        const uint32_t bits = (vn == 3 && vl == 3) ? 16 : (32u >> vl) * (vn + 1);
        return ((cnt * bits + 31) / 32) * 4;
    }
    return 0;
}

uint32_t gif_packet_qwc(const VUState& st, uint32_t addr) {
    uint32_t total = 0;
    for (int guard = 0; guard < 1024; ++guard) {
        uint32_t w[2];  // (VU memory: host-order 32-bit words, the doubleword's low one first on any host)
        std::memcpy(w, &st.mem[((addr + total) & 0x3ff) << 4], 8);
        const uint64_t tag = (static_cast<uint64_t>(w[1]) << 32) | w[0];
        const uint32_t nloop = tag & 0x7fff, flg = (tag >> 58) & 3;
        const uint32_t nreg = ((tag >> 60) & 0xf) == 0 ? 16 : (tag >> 60) & 0xf;
        const uint32_t body = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
        if (trace_on() && guard < 40) std::fprintf(stderr, "[VU1]   tag @%03x %016llx\n", (addr + total) & 0x3ff, static_cast<unsigned long long>(tag));
        total += 1 + body;
        if (((tag >> 15) & 1) != 0 || total >= 0x400) break;
    }
    return std::min(total, 0x400u);
}

void run_interpreter(Vu1& vu, uint32_t start, const std::function<void(uint32_t)>& kick) {
    vu.st.xgkick_hook = kick;
    VuMicroRun run;  // (execute_microprogram, with VU1's wrap of branch targets in its 2048-instruction micro memory)
    run.start(vu.st, vu.micro.data(), vu.micro.size(), start);
    run.set_pc_mask(0x7ff);
    for (uint64_t executed = 0; ++executed <= 2000000 && run.step();) {
    }
    run.finish();
    vu.st.xgkick_hook = nullptr;
}

void flip_buffers(Vu1& vu, uint32_t start) {
    g_last_start = start;
    vu.top = vu.tops;
    vu.itop = vu.itops;
    vu.dbf = !vu.dbf;
    vu.tops = (vu.base + (vu.dbf ? vu.ofst : 0)) & 0x3ff;
    vu.st.xtop = vu.top;
}

void start_program(Vu1& vu, uint32_t start, const KickSink& kick) {
    flip_buffers(vu, start);
    auto kick_at = [&vu, &kick, start](uint32_t addr) {
        const uint32_t qwc = gif_packet_qwc(vu.st, addr);
        if (trace_on()) std::fprintf(stderr, "[VU1] kick %03x qwc %u\n", addr, qwc);
        const uint32_t a0 = addr & 0x3ffu;
        std::vector<uint8_t> wrap_buf;
        const uint8_t* pkt = &vu.st.mem[a0 << 4];
        if (a0 + qwc > 0x400u) {
            wrap_buf.resize(static_cast<size_t>(qwc) * 16);
            for (uint32_t q = 0; q < qwc; ++q) std::memcpy(&wrap_buf[q * 16], &vu.st.mem[((a0 + q) & 0x3ffu) << 4], 16);
            pkt = wrap_buf.data();
        }
        static const char* const pkt_dir = std::getenv("RAC1_VU1_PKTDUMP");  // testing: kicks #20000. as files <dir>/pkt_<n>_s<start>_a<addr>.bin
        if (pkt_dir != nullptr) {
            static int kicks = 0;
            static const int from = std::getenv("RAC1_VU1_PKTFROM") != nullptr ? std::atoi(std::getenv("RAC1_VU1_PKTFROM")) : 20000;
            static const int only_start = std::getenv("RAC1_VU1_PKTSTART") != nullptr ? static_cast<int>(std::strtol(std::getenv("RAC1_VU1_PKTSTART"), nullptr, 16)) : -1;
            if (only_start >= 0 && static_cast<int>(start) != only_start) {
            } else if (qwc < static_cast<uint32_t>(std::getenv("RAC1_VU1_PKTMIN") != nullptr ? std::atoi(std::getenv("RAC1_VU1_PKTMIN")) : 0)) {
            } else if (++kicks >= from && kicks < from + 60) {
                char path[512];
                std::snprintf(path, sizeof path, "%s/pkt_%05d_s%03x_a%03x.bin", pkt_dir, kicks, start, addr);
                if (FILE* f = std::fopen(path, "wb")) { std::fwrite(pkt, 1, static_cast<size_t>(qwc) * 16, f); std::fclose(f); }
            }
        }
        if (g_kick_tap) g_kick_tap(pkt, qwc);
        if (kick) kick(pkt, qwc);
    };
    static const char* const capture_dir = std::getenv("RAC1_VU1_CAPTURE");
    if (capture_dir != nullptr) capture(vu, start, capture_dir);  // testing: the oracle's inputs
    static const bool skip = std::getenv("RAC1_VU1_SKIP") != nullptr;  // testing: nothing drawn by VU1
    if (skip) return;
    static const bool interp_env = std::getenv("RAC1_VU1_INTERP") != nullptr;
    const bool interp = interp_env || g_force_interpreter;
    if (trace_on()) std::fprintf(stderr, "[VU1] start %03x tops %03x\n", start, vu.tops);
    if (!interp) {
        // (the hash of a port's 0x508 instructions is not recomputed at every start: the answer holds until the micro memory is written)
        struct Memo {
            uint32_t start = 0;
            uint64_t gen = 0;
            const Registered* port = nullptr;
        };
        static thread_local Memo memo[32];  // (static: the Wii U build defines thread_local away)
        static thread_local unsigned next = 0;
        const Registered* found = nullptr;
        bool hit = false;
        for (const Memo& e : memo)
            if (e.gen == g_micro_gen.load(std::memory_order_relaxed) && e.start == start) {
                found = e.port;
                hit = true;
                break;
            }
        if (!hit) {
            for (const Registered& r : registry())
                if (start >= r.addr && start < r.addr + r.n && r.addr + r.n <= vu.micro.size() && code_hash(vu.micro.data() + r.addr, r.n) == r.hash) {
                    found = &r;
                    break;
                }
            memo[next++ & 31] = Memo{start, g_micro_gen.load(std::memory_order_relaxed), found};
        }
        if (found != nullptr) {
            struct Observed {  // (host: the walk's observer around the start, StartObserver)
                StartObserver* o;
                Vu1& vu;
                uint32_t start;
                uint64_t hash;
                ~Observed() {
                    if (o != nullptr) o->after(vu, start, hash);
                }
            } observed{g_start_observer, vu, start, found->hash};
            if (observed.o != nullptr) observed.o->before(vu, start, found->hash);
            if (g_start_hook && g_start_hook(vu, start, found->hash, found->fn, kick_at)) return;
            found->fn(vu.st, start, kick_at);
            if (trace_on()) std::fprintf(stderr, "[VU1] ran %s start %03x: vi11 %d vi13 %d\n", found->name, start, vu.st.vi[11], vu.st.vi[13]);
            return;
        }
    }
    // testing (RAC1_VU_FALLBACK=1): the starts no port ran (on the interpreter, or skipped), by start and micro image, printed at exit
    static const bool fb_on = std::getenv("RAC1_VU_FALLBACK") != nullptr;
    if (fb_on) {
        const auto t0 = std::chrono::steady_clock::now();
        const std::pair<uint32_t, uint64_t> key{start, code_hash(vu.micro.data(), static_cast<uint32_t>(vu.micro.size()))};
        if (interp) run_interpreter(vu, start, kick_at);
        const auto ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
        {
            std::lock_guard<std::mutex> lock(g_fb_mutex);
            auto& e = vu1_fallbacks()[key];
            ++e.first;
            e.second += ns;
        }
        if (interp) return;
    }
    if (interp) {
        run_interpreter(vu, start, kick_at);
        return;
    }
    static std::set<uint32_t> logged;
    if (logged.insert(start).second)
        std::fprintf(stderr, "[VU1] no port for the microprogram started at %03x: skipped\n", start);
}

bool vif1_command(Vu1& vu, uint32_t code, const uint8_t* data, const KickSink& kick) {
    const uint32_t cmd = (code >> 24) & 0x7f, imm = code & 0xffff, num = (code >> 16) & 0xff;
    switch (cmd) {
    case 0x01: vu.cl = imm & 0xff; vu.wl = (imm >> 8) & 0xff; return true;  // STCYCL
    case 0x02: vu.ofst = imm & 0x3ff; vu.dbf = false; vu.tops = vu.base; return true;  // OFFSET
    case 0x03: vu.base = imm & 0x3ff; return true;                       // BASE
    case 0x04: vu.itops = imm & 0x3ff; return true;                      // ITOP
    case 0x05: vu.mode = imm & 3; return true;                           // STMOD
    case 0x14: case 0x15: start_program(vu, imm, kick); vu.next_pc = imm; return true;  // MSCAL / MSCALF
    case 0x17: start_program(vu, vu.next_pc, kick); return true;         // MSCNT
    case 0x20: vu.mask = r32(data); return true;                         // STMASK
    case 0x30: for (int i = 0; i < 4; ++i) vu.row[i] = r32(data + 4 * i); return true;  // STROW
    case 0x31: for (int i = 0; i < 4; ++i) vu.col[i] = r32(data + 4 * i); return true;  // STCOL
    case 0x4a: {                                                         // MPG
        const uint32_t n = num == 0 ? 256 : num;
        // testing (RAC1_VU1_DUMP_SETS=<dir>): the microprograms the game uploads, one file per distinct set (consecutive MPGs from address 0),
        // the generators' input: set_<n instructions>_<hash>.bin
        static const char* const dump_dir = std::getenv("RAC1_VU1_DUMP_SETS");
        if (dump_dir != nullptr) {
            struct Dump {
                std::vector<uint64_t> words;
                std::set<uint64_t> done;
                const char* dir = nullptr;
                void flush() {
                    if (words.size() < 16) { words.clear(); return; }
                    const uint64_t h = code_hash(words.data(), static_cast<uint32_t>(words.size()));
                    if (done.insert(h).second) {
                        char path[512];
                        std::snprintf(path, sizeof path, "%s/set_%04zx_%016llx.bin", dir, words.size(), static_cast<unsigned long long>(h));
                        if (FILE* f = std::fopen(path, "wb")) { std::fwrite(words.data(), 8, words.size(), f); std::fclose(f); }
                    }
                    words.clear();
                }
                ~Dump() { flush(); }
            };
            static Dump dump;
            dump.dir = dump_dir;
            if (imm == 0) dump.flush();
            if (dump.words.size() == imm && imm + n <= 0x800) {
                const uint64_t* w = reinterpret_cast<const uint64_t*>(data);
                dump.words.insert(dump.words.end(), w, w + n);
            }
        }
        if (trace_on()) std::fprintf(stderr, "[VU1] MPG at %03x n %u hash %016llx\n", imm, n, static_cast<unsigned long long>(code_hash(reinterpret_cast<const uint64_t*>(data), n)));
        for (uint32_t i = 0; i < n; ++i)  // (an instruction's low word first, as EE memory holds it on any host)
            vu.micro[(imm + i) & 0x7ff] = r32(data + 8 * i) | static_cast<uint64_t>(r32(data + 8 * i + 4)) << 32;
        ++g_micro_gen;
        return true;
    }
    default:
        if (cmd >= 0x60) {
            unpack(vu, code, data);
            return true;
        }
        return false;
    }
}

}  // namespace rac1::vu1
