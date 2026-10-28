// A sampling profiler for the console (RAC1_WIIU_SAMPLE=<microseconds>): a periodic alarm on each sampled core takes the
// interrupted thread's pc, lr and a few return addresses of its stack's back chain at each tick, into a fixed buffer per core:
// no allocation, no lock in the callback. At the end samples.bin goes to the SD card for ports/wiiu/tools/samples.py, which
// maps the addresses on the ELF's symbols (self time, and the inclusive time from the back chain).
//
// The cores: an alarm fires on the core it was set from (coreinit keeps a queue of alarms per core, run from that core's
// decrementer interrupt), and its callback gets the context of the thread that interrupt stopped - so each sampled core's
// alarm is set by a short thread pinned to that core. RAC1_WIIU_SAMPLE_CORES lists them ("2", "1,2", "0,1,2"); by default
// the game thread's core (RAC1_WIIU_GAME_CORE, 2: the game's update) or, with the game thread off, the main thread's (1).
// Each sample keeps the interrupted thread (its OSThread, named in the file's thread table) and each core counts the ticks
// that ran elsewhere than on it (that rule's check: 0 expected).
//
// The mapping: the file carries the addresses as loaded of a few functions spread over the code (the self-test: samples.py
// finds each one's own name back at that address minus the code's relocation, or the ELF is not this run's build) and the
// names coreinit gives (OSGetSymbolName) to the addresses outside our code (Cafe OS's libraries).
// RAC1_WIIU_SAMPLE_AFTER=<seconds>: no sample before then (the loading left out). RAC1_WIIU_SAMPLE_MB: the buffers' size
// (default 8 MB, 32 bytes a sample, shared by the cores). RAC1_WIIU_SAMPLE_NAMES=0: no OSGetSymbolName at the end.
#include <coreinit/alarm.h>
#include <coreinit/context.h>
#include <coreinit/core.h>
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memory.h>
#include <coreinit/memorymap.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" void wiiu_sampler_anchor() {}

// the self-test's functions, spread over .text: the port's (near its start), the game's (the generated code, its middle), newlib's
// (near its end)
int rac1_orig_main(int argc, char** argv);  // src/host/orig_main.cpp
bool rac1_wiiu_game_thread();               // debug_menu.cpp
namespace rac1 {
void hero_shadow_update();
void particle_0e_update(unsigned int);
}  // namespace rac1

namespace {
constexpr int kDepth = 5;  // (the back chain's return addresses kept)
struct Sample {
    uint32_t pc, lr, thread;  // (thread: the interrupted thread's OSThread)
    uint32_t ret[kDepth];
};
static_assert(sizeof(Sample) == 32);
struct Core {
    OSAlarm alarm;
    Sample* buf = nullptr;
    uint32_t cap = 0;
    int core = -1;
    volatile uint32_t n = 0, lost = 0, idle = 0, elsewhere = 0;
};
Core g_core[3];
uint32_t g_period_us = 0;
OSTime g_from = 0;
bool g_on = false;
// the memory the back chain may be read in (the callback runs in an interrupt: a bad read there is a DSI): the data from
// 0x10000000 up (checked page by page at the start), MEM2's arena (the heaps: the game thread's and the coroutines' stacks), the
// main thread's stack
uint32_t g_ok_lo[3], g_ok_hi[3];

inline bool readable(uint32_t a, uint32_t len) {
    for (int i = 0; i < 3; ++i)
        if (a >= g_ok_lo[i] && a + len <= g_ok_hi[i] && a + len > a) return true;
    return false;
}

void tick(OSAlarm* alarm, OSContext* ctx) {
    Core& c = *static_cast<Core*>(OSGetAlarmUserData(alarm));
    if (OSGetTime() < g_from) return;
    if (ctx == nullptr) {
        c.idle = c.idle + 1;
        return;
    }
    const uint32_t n = c.n;
    if (n >= c.cap) {
        c.lost = c.lost + 1;
        return;
    }
    if (static_cast<int>(OSGetCoreId()) != c.core) c.elsewhere = c.elsewhere + 1;
    Sample& s = c.buf[n];
    const uint32_t a_ctx = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx));
    s.pc = ctx->srr0;
    s.lr = ctx->lr;
    // the interrupted thread: the context's OSThread (its first member) when it is one, else the core's current thread; its
    // own stack readable too
    const OSThread* t = readable(a_ctx, sizeof(OSThread)) && reinterpret_cast<const OSThread*>(ctx)->tag == OS_THREAD_TAG
                            ? reinterpret_cast<const OSThread*>(ctx)
                            : OSGetCurrentThread();
    s.thread = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t));
    uint32_t t_lo = 0, t_hi = 0;
    if (t != nullptr && t->tag == OS_THREAD_TAG) {
        const uint32_t a = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t->stackEnd));
        const uint32_t b = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t->stackStart));
        t_lo = std::min(a, b);
        t_hi = std::max(a, b);
    }
    const auto ok = [&](uint32_t a) { return (a & 3) == 0 && ((a >= t_lo && a + 8 <= t_hi) || readable(a, 8)); };
    // the back chain (as log_backtrace): each frame's word 0 the caller's frame, whose word 1 is the return address into it
    uint32_t sp = ctx->gpr[1];
    int k = 0;
    while (k < kDepth && ok(sp)) {
        const uint32_t next = *reinterpret_cast<const volatile uint32_t*>(sp);
        if (next <= sp || next - sp > (1u << 20) || !ok(next)) break;
        s.ret[k++] = *reinterpret_cast<const volatile uint32_t*>(next + 4);
        sp = next;
    }
    for (; k < kDepth; ++k) s.ret[k] = 0;
    c.n = n + 1;
}

// the thread setting a core's alarm, on that core (an alarm fires on the core it was set from)
OSThread g_arm __attribute__((aligned(16)));
uint8_t g_arm_stack[16 * 1024] __attribute__((aligned(16)));
int arm_main(int i, const char**) {
    Core& c = g_core[i];
    const OSTime period = static_cast<OSTime>(OSMicrosecondsToTicks(g_period_us));
    OSSetPeriodicAlarm(&c.alarm, OSGetTime() + period, period, tick);
    return static_cast<int>(OSGetCoreId());
}

uint32_t addr_of(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
template <typename F>
uint32_t fn_addr(F* f) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(f));
}

// a fixed-size name record: the address, then the name (zero-padded)
struct Named {
    uint32_t addr;
    char name[60];
};
static_assert(sizeof(Named) == 64);
Named named(uint32_t addr, const char* s) {
    Named r{};
    r.addr = addr;
    if (s != nullptr) std::strncpy(r.name, s, sizeof r.name - 1);
    return r;
}
// coreinit's name of an address ("" when it has none)
Named os_name(uint32_t addr) {
    char buf[60] = {};
    OSGetSymbolName(addr, buf, sizeof buf);
    buf[sizeof buf - 1] = 0;
    return named(addr, buf);
}
}  // namespace

// the alarms set on the sampled cores; 0 or no setting: off
void wiiu_sampler_start() {
    const char* s = std::getenv("RAC1_WIIU_SAMPLE");
    const int us = s != nullptr ? std::atoi(s) : 0;
    if (us <= 0) return;
    g_period_us = static_cast<uint32_t>(us);
    // the cores
    bool want[3] = {false, false, false};
    if (const char* cs = std::getenv("RAC1_WIIU_SAMPLE_CORES"); cs != nullptr && cs[0] != 0) {
        for (const char* p = cs; *p != 0; ++p)
            if (*p >= '0' && *p <= '2') want[*p - '0'] = true;
    } else if (rac1_wiiu_game_thread()) {
        const char* ce = std::getenv("RAC1_WIIU_GAME_CORE");
        const int gc = ce != nullptr ? std::atoi(ce) : 2;
        want[gc >= 0 && gc <= 2 ? gc : 2] = true;
    } else {
        want[OSGetCoreId()] = true;
    }
    const int cores = static_cast<int>(want[0]) + static_cast<int>(want[1]) + static_cast<int>(want[2]);
    if (cores == 0) return;
    // the memory the back chain may be read in
    uint32_t m2 = 0, m2_size = 0;
    if (OSGetMemBound(OS_MEM2, &m2, &m2_size) != 0) m2 = m2_size = 0;
    uint32_t data_hi = 0x10000000;
    while (data_hi < 0x20000000 && (m2 == 0 || data_hi < m2) && OSIsAddressValid(data_hi)) data_hi += 0x1000;
    g_ok_lo[0] = 0x10000000;
    g_ok_hi[0] = data_hi;
    g_ok_lo[1] = m2;
    g_ok_hi[1] = m2 + m2_size;
    const OSThread* self = OSGetCurrentThread();
    g_ok_lo[2] = std::min(addr_of(self->stackEnd), addr_of(self->stackStart));
    g_ok_hi[2] = std::max(addr_of(self->stackEnd), addr_of(self->stackStart));
    // the buffers
    const char* mbs = std::getenv("RAC1_WIIU_SAMPLE_MB");
    uint32_t bytes = static_cast<uint32_t>(std::clamp(mbs != nullptr ? std::atoi(mbs) : 8, 1, 64)) << 20;
    void* mem = nullptr;
    while (bytes >= (1u << 20) && (mem = MEMAllocFromDefaultHeapEx(bytes, 64)) == nullptr) bytes >>= 1;
    if (mem == nullptr) {
        std::printf("[wiiu] sampling: no memory for the samples\n");
        return;
    }
    const uint32_t per = bytes / static_cast<uint32_t>(cores) / sizeof(Sample);
    Sample* next = static_cast<Sample*>(mem);
    const char* after = std::getenv("RAC1_WIIU_SAMPLE_AFTER");
    g_from = OSGetTime() + (after != nullptr ? static_cast<OSTime>(OSSecondsToTicks(std::atoi(after))) : 0);
    for (int i = 0; i < 3; ++i) {
        if (!want[i]) continue;
        Core& c = g_core[i];
        c.core = i;
        c.buf = next;
        c.cap = per;
        next += per;
        OSCreateAlarm(&c.alarm);
        OSSetAlarmUserData(&c.alarm, &c);
        int on = -1;
        if (OSCreateThread(&g_arm, arm_main, i, nullptr, g_arm_stack + sizeof g_arm_stack, sizeof g_arm_stack, 0,
                           static_cast<OSThreadAttributes>(OS_THREAD_ATTRIB_AFFINITY_CPU0 << i))) {
            OSResumeThread(&g_arm);
            OSJoinThread(&g_arm, &on);
        }
        std::printf("[wiiu] sampling core %d every %u us (its alarm set on core %d), %u samples at most\n", i, g_period_us, on,
                    static_cast<unsigned>(per));
    }
    g_on = true;
    std::printf("[wiiu] sampling: anchor %08x, back chain readable in %08x-%08x %08x-%08x %08x-%08x\n", addr_of(reinterpret_cast<void*>(&wiiu_sampler_anchor)),
                static_cast<unsigned>(g_ok_lo[0]), static_cast<unsigned>(g_ok_hi[0]), static_cast<unsigned>(g_ok_lo[1]),
                static_cast<unsigned>(g_ok_hi[1]), static_cast<unsigned>(g_ok_lo[2]), static_cast<unsigned>(g_ok_hi[2]));
}

// The alarms stopped and samples.bin written (big-endian, as everything here):
//   "RAC1SMP2", then u32 [8]: the period (us), the back chain's depth, the anchors, the cores, the threads, the names, 0, 0;
//   the anchors (Named: the address as loaded, the mangled name; then a Named of coreinit's name for that address);
//   per core: u32 [8] (the core, the samples, lost, idle ticks, ticks run on another core, 0, 0, 0) then its samples (Sample);
//   the threads (Named: the context's address, the OSThread's name); the names (Named: an address outside our code, coreinit's name)
void wiiu_sampler_stop() {
    if (!g_on) return;
    for (Core& c : g_core)
        if (c.buf != nullptr) OSCancelAlarm(&c.alarm);
    g_on = false;
    OSSleepTicks(OSMillisecondsToTicks(2));  // (a tick under way on another core)
    const OSTime t0 = OSGetTime();
    // the self-test's functions
    const Named anchors[] = {
        named(fn_addr(&wiiu_sampler_anchor), "wiiu_sampler_anchor"),
        named(fn_addr(&rac1_orig_main), "_Z14rac1_orig_mainiPPc"),
        named(fn_addr(&rac1::hero_shadow_update), "_ZN4rac118hero_shadow_updateEv"),
        named(fn_addr(&rac1::particle_0e_update), "_ZN4rac118particle_0e_updateEj"),
        named(fn_addr(&std::memcpy), "memcpy"),
        named(fn_addr(&std::strtod), "strtod"),
    };
    constexpr uint32_t kAnchors = sizeof anchors / sizeof anchors[0];
    // the threads seen, and the addresses outside our code (Cafe OS's: below 0x02000000, where no RPX is loaded)
    // (the pcs' first: at most 8192 named, samples.py names the others by the nearest named address below)
    std::vector<uint32_t> threads, os, os_calls;
    int cores = 0;
    for (const Core& c : g_core) {
        if (c.buf == nullptr) continue;
        ++cores;
        for (uint32_t i = 0; i < c.n; ++i) {
            const Sample& s = c.buf[i];
            if (std::find(threads.begin(), threads.end(), s.thread) == threads.end() && threads.size() < 256) threads.push_back(s.thread);
            if (s.pc < 0x02000000) os.push_back(s.pc);
            if (s.lr < 0x02000000) os_calls.push_back(s.lr);
            for (uint32_t r : s.ret)
                if (r != 0 && r < 0x02000000) os_calls.push_back(r);
        }
    }
    for (std::vector<uint32_t>* v : {&os, &os_calls}) {
        std::sort(v->begin(), v->end());
        v->erase(std::unique(v->begin(), v->end()), v->end());
    }
    const size_t pcs = os.size();
    for (uint32_t a : os_calls)
        if (os.size() < 8192 && !std::binary_search(os.begin(), os.begin() + static_cast<std::ptrdiff_t>(pcs), a)) os.push_back(a);
    const char* nm = std::getenv("RAC1_WIIU_SAMPLE_NAMES");
    if (nm != nullptr && nm[0] == '0') os.clear();
    if (os.size() > 8192) os.resize(8192);
    std::FILE* f = std::fopen("samples.bin", "wb");
    if (f == nullptr) return;
    const uint32_t head[8] = {g_period_us, kDepth, kAnchors, static_cast<uint32_t>(cores), static_cast<uint32_t>(threads.size()),
                              static_cast<uint32_t>(os.size()), 0, 0};
    std::fwrite("RAC1SMP2", 1, 8, f);
    std::fwrite(head, 4, 8, f);
    for (const Named& a : anchors) {
        const Named o = os_name(a.addr);
        std::fwrite(&a, sizeof a, 1, f);
        std::fwrite(&o, sizeof o, 1, f);
    }
    uint32_t total = 0;
    for (const Core& c : g_core) {
        if (c.buf == nullptr) continue;
        const uint32_t n = c.n;
        const uint32_t ch[8] = {static_cast<uint32_t>(c.core), n, c.lost, c.idle, c.elsewhere, 0, 0, 0};
        std::fwrite(ch, 4, 8, f);
        std::fwrite(c.buf, sizeof(Sample), n, f);
        total += n;
        std::printf("[wiiu] core %d: %u samples, %u lost, %u idle, %u run on another core\n", c.core, static_cast<unsigned>(n),
                    static_cast<unsigned>(c.lost), static_cast<unsigned>(c.idle), static_cast<unsigned>(c.elsewhere));
    }
    for (uint32_t t : threads) {
        const OSThread* th = reinterpret_cast<const OSThread*>(static_cast<uintptr_t>(t));
        const char* name = nullptr;
        if (OSIsAddressValid(t) && OSIsAddressValid(t + sizeof(OSThread) - 1) && th->tag == OS_THREAD_TAG && th->name != nullptr &&
            OSIsAddressValid(addr_of(th->name)))
            name = th->name;
        const Named r = named(t, name != nullptr ? name : "");
        std::fwrite(&r, sizeof r, 1, f);
    }
    for (uint32_t a : os) {
        const Named r = os_name(a);
        std::fwrite(&r, sizeof r, 1, f);
    }
    std::fclose(f);
    std::printf("[wiiu] %u samples, %u threads, %u system addresses named in samples.bin (%u ms)\n", static_cast<unsigned>(total),
                static_cast<unsigned>(threads.size()), static_cast<unsigned>(os.size()),
                static_cast<unsigned>(OSTicksToMilliseconds(OSGetTime() - t0)));
}
