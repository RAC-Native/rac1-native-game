#include <runtime/vu/vu0.h>

#include <runtime/kernel/ee_memory.h>
#include <runtime/vu/vu_prof.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace rac1::vu0 {

namespace {

// The VU profiler's clock: the TSC on x86 (cheap enough per step), nanoseconds elsewhere
inline uint64_t prof_ticks() {
#if defined(__x86_64__) || defined(__i386__)
    return __builtin_ia32_rdtsc();
#else
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
// Testing (RAC1_VU0_CHAIN=1): a hash of the VU0 state where each run stops (it waits on the EE, or ends), every 16th stop -- the
// same for two runs of the game that did the same, whichever way the program ran (the exact ports or the fast ones)
uint64_t g_chain = 0xcbf29ce484222325ull;
uint64_t g_chain_n = 0;
bool chain_on() {
    static const bool on = std::getenv("RAC1_VU0_CHAIN") != nullptr;
    return on;
}
void chain_add(const VUState& st) {
    if ((g_chain_n++ & 15) != 0) return;
    auto fnv = [](uint64_t h, const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 0x100000001b3ull;
        return h;
    };
    uint64_t h = g_chain;
    h = fnv(h, st.vf.data(), sizeof(st.vf));
    h = fnv(h, st.vi.data(), sizeof(st.vi));
    h = fnv(h, &st.acc, sizeof(st.acc));
    h = fnv(h, &st.q, 4);
    h = fnv(h, &st.i, 4);
    h = fnv(h, &st.macflag, 12);
    h = fnv(h, st.mem.data(), 4096);
    g_chain = h;
}
uint64_t g_micro_gen = 1;  // counts the writes of the micro memory and the registered ports: port_for's memo is valid for one value
}

namespace detail {
Vu0 g_instance = [] {
    Vu0 x;
    x.st.vf[0].f[3] = 1.0f;
    return x;
}();
}  // namespace detail

namespace {
uint64_t fnv_words(const uint64_t* words, uint32_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < n; ++i)
        for (int b = 0; b < 8; ++b) {
            h ^= (words[i] >> (8 * b)) & 0xff;
            h *= 0x100000001b3ull;
        }
    return h;
}
// Testing (RAC1_VU0_DUMP_SETS=<dir>): every distinct MPG the game sends to VU0, one file each (the generators' input):
// <dir>/vu0set_<micro address>_<n instructions>_<fnv1a64 of them>.bin
void dump_set(uint32_t at, uint32_t num, const std::vector<uint32_t>& words, size_t i) {
    static const char* const dir = std::getenv("RAC1_VU0_DUMP_SETS");
    if (dir == nullptr || i + 2 * static_cast<size_t>(num) > words.size()) return;
    std::vector<uint64_t> w(num);
    for (uint32_t k = 0; k < num; ++k) w[k] = words[i + 2 * k] | (static_cast<uint64_t>(words[i + 2 * k + 1]) << 32);
    const uint64_t h = fnv_words(w.data(), num);
    static std::set<uint64_t> done;
    if (!done.insert(h).second) return;
    char path[512];
    std::snprintf(path, sizeof path, "%s/vu0set_%03x_%03x_%016llx.bin", dir, at, num, static_cast<unsigned long long>(h));
    if (FILE* f = std::fopen(path, "wb")) {
        std::fwrite(w.data(), 8, num, f);
        std::fclose(f);
    }
}

// Testing (RAC1_VU_FALLBACK=1): the VU0 programs that ran on the interpreter (no port registered for their code), by start
// address and the hash of the whole micro memory: starts, instructions, time; printed at exit.
struct Fallback {
    uint64_t starts = 0, insns = 0, ns = 0;
};
std::map<std::pair<uint32_t, uint64_t>, Fallback>& fallbacks() {
    static std::map<std::pair<uint32_t, uint64_t>, Fallback> m;
    return m;
}
Fallback* g_fb_cur = nullptr;  // the running program's entry while it runs on the interpreter
bool fallback_on() {
    static const bool on = std::getenv("RAC1_VU_FALLBACK") != nullptr;
    return on;
}
// a stretch of a program's run: its instructions and time added to the fallback entry, if any
struct FbScope {
    Fallback* e;
    uint64_t i0;
    std::chrono::steady_clock::time_point t0;
    explicit FbScope(const VuMicroRun& r) : e(g_fb_cur), i0(r.pipe().issued) {
        if (e != nullptr) t0 = std::chrono::steady_clock::now();
    }
    void done(const VuMicroRun& r) {
        if (e == nullptr) return;
        e->insns += r.pipe().issued - i0;
        e->ns += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
        e = nullptr;
    }
};

// Testing (RAC1_VU0_CAPTURE=<dir>): the state a VU0 program starts on (registers, data and micro memory), at most 64 per
// (start, micro image), spread over the calls (every 3rd), for the oracle's proof of the ported programs. vu1.cpp's
// capture format with VU0's sizes: header 'VU0' 1, start, 0, 0; vf; vi; ACC; Q I R P; MAC status clip 0; 4 KB data; 4 KB micro.
void capture(const Vu0& v, uint32_t addr) {
    static const char* const dir = std::getenv("RAC1_VU0_CAPTURE");
    if (dir == nullptr) return;
    static std::map<std::pair<uint32_t, uint64_t>, std::pair<int, int>> seen;
    static int total = 0;
    const uint64_t h = fnv_words(v.micro.data(), static_cast<uint32_t>(v.micro.size()));
    auto& e = seen[{addr, h}];
    if (total >= 4000 || e.second >= 64 || (e.first++ % 3) != 0) return;
    ++e.second;
    char path[512];
    std::snprintf(path, sizeof path, "%s/vu0_%016llx_%03x_%04d.bin", dir, static_cast<unsigned long long>(h), addr, total++);
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) return;
    const VUState& st = v.st;
    const uint32_t hdr[4] = {0x01305556u, addr, 0, 0};
    std::fwrite(hdr, 4, 4, f);
    std::fwrite(st.vf.data(), sizeof(VUVector), 32, f);
    std::fwrite(st.vi.data(), 2, 16, f);
    std::fwrite(&st.acc, sizeof(VUVector), 1, f);
    const float fr[4] = {st.q, st.i, st.r, st.p};
    std::fwrite(fr, 4, 4, f);
    const uint32_t fl[4] = {st.macflag, st.statusflag, st.clipflag, 0};
    std::fwrite(fl, 4, 4, f);
    std::fwrite(st.mem.data(), 1, 4096, f);
    std::fwrite(v.micro.data(), 8, v.micro.size(), f);
    std::fclose(f);
}
}  // namespace

void vif0_send_chain(uint32_t tadr, bool tte) {
    std::vector<uint32_t> words;
    for (int guard = 0; guard < 4096; ++guard) {
        const uint64_t tag = ee_read<uint64_t>(tadr);
        const uint32_t qwc = static_cast<uint32_t>(tag & 0xffff), id = static_cast<uint32_t>(tag >> 28) & 7;
        const uint32_t ta = static_cast<uint32_t>(tag >> 32) & 0x7ffffff0u;
        if (tte) {
            words.push_back(ee_read<uint32_t>(tadr + 8));
            words.push_back(ee_read<uint32_t>(tadr + 12));
        }
        uint32_t from = 0;
        bool end = false;
        switch (id) {
        case 0: from = ta; end = true; break;                          // refe
        case 1: from = tadr + 16; tadr += 16 + qwc * 16; break;         // cnt
        case 2: from = tadr + 16; tadr = ta; break;                     // next
        case 3: case 4: from = ta; tadr += 16; break;                   // ref / refs
        case 7: from = tadr + 16; end = true; break;                    // end
        default: end = true; break;
        }
        for (uint32_t k = 0; k < qwc * 4; ++k) words.push_back(ee_read<uint32_t>(from + 4 * k));
        if (end) break;
    }
    auto& mm = global().micro;
    for (size_t i = 0; i < words.size();) {
        const uint32_t code = words[i++];
        const uint32_t cmd = (code >> 24) & 0x7f;
        if (cmd == 0x4a) {  // MPG
            uint32_t num = (code >> 16) & 0xff;
            if (num == 0) num = 256;
            const uint32_t at = code & 0xffff;
            dump_set(at, num, words, i);
            for (uint32_t k = 0; k < num && i + 1 < words.size(); ++k, i += 2)
                if (at + k < mm.size()) {
                    mm[at + k] = words[i] | (static_cast<uint64_t>(words[i + 1]) << 32);
                    ++g_micro_gen;
                }
        } else if (cmd != 0 && cmd != 1 && cmd != 0x10 && cmd != 0x11 && cmd != 0x13) {
            static bool once = false;
            if (!once) std::fprintf(stderr, "[vu0] VIF0 code %08x not handled\n", code);
            once = true;
            break;
        }
    }
}

namespace {
struct Port {
    uint32_t addr, n;
    uint64_t hash;
    VuNativeStep step;
    const char* name;
};
std::vector<Port>& ports() {
    static std::vector<Port> p;
    return p;
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
VuNativeStep port_for_uncached(uint32_t addr) {
    const auto& m = global().micro;
    for (const Port& p : ports())
        if (addr >= p.addr && addr < p.addr + p.n && p.addr + p.n <= m.size() && code_hash(m.data() + p.addr, p.n) == p.hash) return p.step;
    return nullptr;
}
// (the hash of a program's few hundred instructions is not recomputed at every start: the answer holds until the micro memory is written)
VuNativeStep port_for(uint32_t addr) {
    static const bool interp = std::getenv("RAC1_VU0_INTERP") != nullptr;  // testing: the interpreter always
    if (interp) return nullptr;
    struct Memo {
        uint32_t addr = 0;
        uint64_t gen = 0;
        VuNativeStep step = nullptr;
    };
    static Memo memo[16];
    static unsigned next = 0;
    for (const Memo& e : memo)
        if (e.gen == g_micro_gen && e.addr == addr) return e.step;
    const VuNativeStep step = port_for_uncached(addr);
    memo[next++ & 15] = Memo{addr, g_micro_gen, step};
    return step;
}
}  // namespace

void register_program(uint32_t addr, uint32_t n, uint64_t hash, VuNativeStep step, const char* name) {
    for (const Port& p : ports())
        if (p.step == step) return;
    ports().push_back(Port{addr, n, hash, step, name});
    ++g_micro_gen;
}

bool running() { return global().run.running(); }

uint64_t chain_hash() { return g_chain; }

void fallback_report() {
    if (!fallback_on()) return;
    for (const auto& kv : fallbacks())
        std::fprintf(stderr, "[vu-fallback] vu0 start %03x micro %016llx: %llu starts, %llu instructions, %.1f ms on the interpreter\n", kv.first.first,
                     static_cast<unsigned long long>(kv.first.second), static_cast<unsigned long long>(kv.second.starts), static_cast<unsigned long long>(kv.second.insns),
                     static_cast<double>(kv.second.ns) / 1e6);
    if (fallbacks().empty()) std::fprintf(stderr, "[vu-fallback] vu0: every program ran as a port\n");
}

void drain() {
    Vu0& v = global();
    if (!v.run.running()) return;
    vuprof::Scope prof(0);
    FbScope fb(v.run);
    const uint64_t i0 = v.run.pipe().issued;
    for (uint64_t n = 0; n < 50'000'000 && v.run.step(); ++n) {
    }
    v.run.finish();
    prof.insns = v.run.pipe().issued - i0;
    fb.done(v.run);
}

void run_until_blocked() {
    Vu0& v = global();
    vuprof::Scope prof(0);
    const uint64_t i0 = v.run.pipe().issued;
    FbScope fb(v.run);
    struct Done {
        Vu0& v;
        vuprof::Scope& p;
        uint64_t i0;
        FbScope& fb;
        ~Done() {
            p.insns = v.run.pipe().issued - i0;
            fb.done(v.run);
            if (chain_on()) chain_add(v.st);
        }
    } done{v, prof, i0, fb};
    // a backward jump that finds the registers (vi, vf, ACC) as they were the last time it was
    // taken: the program spins waiting for the EE
    struct Seen {
        uint32_t target;
        std::array<int16_t, 16> vi;
        std::array<VUVector, 32> vf;
        VUVector acc;
    };
    static Seen seen[8];
    int nseen = 0;
    for (uint64_t n = 0; n < 50'000'000 && v.run.running(); ++n) {
        const uint32_t before = v.run.pc();
        const bool stepped = [&] {
            if (!vuprof::enabled()) return v.run.step();
            const uint64_t t0 = prof_ticks(), i0 = v.run.pipe().issued;
            const bool r = v.run.step();
            vuprof::counter(3).ns += prof_ticks() - t0;  // (TSC ticks here)
            ++vuprof::counter(3).runs;
            if (v.run.pipe().issued - i0 == 1) ++vuprof::counter(5).runs;  // (a call that ran one instruction: the exact step function)
            return r;
        }();
        if (!stepped) {
            v.run.finish();
            return;
        }
        const uint32_t after = v.run.pc();
        if (after < before || v.run.pipe().back_jump) {  // (a fast port runs on to its backward jump: it says so)
            int k = 0;
            while (k < nseen && seen[k].target != after) ++k;
            if (k < nseen && seen[k].vi == v.st.vi && std::memcmp(seen[k].vf.data(), v.st.vf.data(), sizeof(seen[k].vf)) == 0 &&
                std::memcmp(&seen[k].acc, &v.st.acc, sizeof(VUVector)) == 0)
                return;
            if (k == nseen) {
                if (nseen == 8) k = 0;  // (more loops than slots: the oldest reused)
                else ++nseen;
            }
            seen[k].target = after;
            seen[k].vi = v.st.vi;
            seen[k].vf = v.st.vf;
            seen[k].acc = v.st.acc;
        }
    }
}

void call(uint32_t addr) {
    Vu0& v = global();
    drain();
    if (vuprof::enabled()) ++vuprof::starts()[addr];
    const uint64_t t0 = vuprof::enabled() ? prof_ticks() : 0;
    const VuNativeStep port = port_for(addr);
    if (vuprof::enabled()) {
        vuprof::counter(4).ns += prof_ticks() - t0;
        ++vuprof::counter(4).runs;
    }
    capture(v, addr);
    g_fb_cur = nullptr;
    if (port == nullptr && fallback_on()) {
        Fallback& e = fallbacks()[{addr, fnv_words(v.micro.data(), static_cast<uint32_t>(v.micro.size()))}];
        ++e.starts;
        g_fb_cur = &e;
    }
    v.run.start(v.st, v.micro.data(), v.micro.size(), addr, port);
    run_until_blocked();
}

}  // namespace rac1::vu0
