// A sampling profiler for the console (RAC1_WIIU_SAMPLE=<microseconds>): a periodic alarm on the game's core takes the
// interrupted thread's pc and lr at each tick (the alarm's callback gets that thread's context), into two fixed arrays: no
// allocation, no lock in the callback. At the end samples.bin goes to the SD card for ports/wiiu/tools/samples.py, which
// maps the addresses on the ELF's symbols (the file's head gives the address of wiiu_sampler_anchor as loaded, to find the
// code's relocation).
#include <coreinit/alarm.h>
#include <coreinit/context.h>
#include <coreinit/debug.h>
#include <coreinit/time.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" void wiiu_sampler_anchor() {}

namespace {
constexpr uint32_t kMax = 1u << 19;  // (512 Ki samples: ~100 s at 200 us)
uint32_t g_pc[kMax], g_lr[kMax];
volatile uint32_t g_n = 0, g_lost = 0;
OSTime g_from = 0;  // (RAC1_WIIU_SAMPLE_AFTER=<seconds>: no sample before then, the loading's left out)
OSAlarm g_alarm;
bool g_on = false;

void tick(OSAlarm*, OSContext* ctx) {
    if (ctx == nullptr || OSGetTime() < g_from) return;
    const uint32_t n = g_n;
    if (n >= kMax) {
        g_lost = g_lost + 1;
        return;
    }
    g_pc[n] = ctx->srr0;
    g_lr[n] = ctx->lr;
    g_n = n + 1;
}
}  // namespace

// the alarm set on the calling thread's core (the game's); 0 or no setting: off
void wiiu_sampler_start() {
    const char* s = std::getenv("RAC1_WIIU_SAMPLE");
    const int us = s != nullptr ? std::atoi(s) : 0;
    if (us <= 0) return;
    OSCreateAlarm(&g_alarm);
    const OSTime period = static_cast<OSTime>(OSMicrosecondsToTicks(us));
    const char* after = std::getenv("RAC1_WIIU_SAMPLE_AFTER");
    g_from = OSGetTime() + (after != nullptr ? static_cast<OSTime>(OSSecondsToTicks(std::atoi(after))) : 0);
    OSSetPeriodicAlarm(&g_alarm, OSGetTime() + period, period, tick);
    g_on = true;
    std::printf("[wiiu] sampling every %d us, anchor %08x\n", us, static_cast<unsigned>(reinterpret_cast<uintptr_t>(&wiiu_sampler_anchor)));
}

// the alarm stopped and samples.bin written: "RAC1SMP1", the anchor's address, the count, the samples lost, then pc / lr pairs
void wiiu_sampler_stop() {
    if (!g_on) return;
    OSCancelAlarm(&g_alarm);
    g_on = false;
    const uint32_t n = g_n;
    std::FILE* f = std::fopen("samples.bin", "wb");
    if (f == nullptr) return;
    const uint32_t head[4] = {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&wiiu_sampler_anchor)), n, g_lost, 0};
    std::fwrite("RAC1SMP1", 1, 8, f);
    std::fwrite(head, 4, 4, f);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t pair[2] = {g_pc[i], g_lr[i]};
        std::fwrite(pair, 4, 2, f);
    }
    std::fclose(f);
    std::printf("[wiiu] %u samples (%u lost) in samples.bin\n", static_cast<unsigned>(n), static_cast<unsigned>(g_lost));
}
