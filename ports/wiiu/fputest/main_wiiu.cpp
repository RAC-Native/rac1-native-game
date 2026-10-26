// The Wii U side of the float check (see fputest_core.h): prints the hashes on the screen and over UDP, writes
// them to rac1-native/fputest.txt on the SD card, and goes back to the menu after a few seconds.
#include "fputest_core.h"
#include "denorm_cases.h"

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <sysapp/launch.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

#include <cstdio>
#include <sys/stat.h>

int main() {
    WHBProcInit();
    WHBLogModuleInit();
    WHBLogUdpInit();
    WHBLogConsoleInit();
    WHBLogPrintf("rac1-fputest: start, fpscr %08x", rac1::ps2fpu::ppc_fpscr());

    rac1::ps2fpu::enter_ee_rounding();
    const OSTime t0 = OSGetTime();
    const auto r = fputest::run(1 << 20);
    const OSTime t1 = OSGetTime();
    uint32_t direct = 0;
    const uint32_t co = fputest::coroutine_check(1000, &direct);
    char line[160];
    std::snprintf(line, sizeof line, "add %08x mul %08x div %08x sqrt %08x denorm %08x vendor %08x", r.add, r.mul,
                  r.div, r.sqrt, r.denorm, r.vendor);
    char line2[100];
    std::snprintf(line2, sizeof line2, "coroutine %08x direct %08x %s", co, direct, co == direct ? "same" : "DIFFERENT");
    WHBLogPrintf("rac1-fputest: fpscr %08x, %u ms", rac1::ps2fpu::ppc_fpscr(),
                 static_cast<unsigned>(OSTicksToMilliseconds(t1 - t0)));
    WHBLogPrintf("rac1-fputest: %s", line);
    WHBLogPrintf("rac1-fputest: %s", line2);
    fputest::denorm_report([](const char* l) { WHBLogPrintf("rac1-fputest: %s", l); });
    mkdir("fs:/vol/external01/rac1-native", 0777);
    if (FILE* f = std::fopen("fs:/vol/external01/rac1-native/fputest.txt", "w")) {
        std::fprintf(f, "%s\n%s\n", line, line2);
        std::fclose(f);
    }

    const OSTime start = OSGetTime();
    bool exiting = false;
    while (WHBProcIsRunning()) {
        if (!exiting && OSTicksToSeconds(OSGetTime() - start) >= 5) {
            WHBLogPrintf("rac1-fputest: auto-exit");
            exiting = true;
            SYSLaunchMenu();
        }
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(16));
    }
    WHBLogPrintf("rac1-fputest: exit");
    WHBLogConsoleFree();
    WHBLogUdpDeinit();
    WHBLogModuleDeinit();
    WHBProcShutdown();
    return 0;
}
