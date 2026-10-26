// Wii U smoke test: logs over UDP (port 4405) and on the screen, and writes a file to the SD.
// It returns to the Wii U Menu by itself after a few seconds (argv[1], default 5;
// 0 = wait for HOME), so the console is always ready for the next launch.
//
// The screen is WHB's log console: it keeps its buffers in MEM1 and gives the screen back
// when ProcUI releases the foreground, which a hand-made OSScreen loop must also do or the
// switch back to the menu never completes.
#include <coreinit/core.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <sysapp/launch.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define OUT_DIR  "fs:/vol/external01/rac1-native"
#define OUT_FILE OUT_DIR "/hello.txt"

int main(int argc, char **argv)
{
    WHBProcInit();
    // Aroma's LoggingModule may be missing: always broadcast on UDP 4405 too.
    WHBLogModuleInit();
    WHBLogUdpInit();
    WHBLogConsoleInit();

    int autoExitSeconds = argc > 1 ? atoi(argv[1]) : 5;
    WHBLogPrintf("rac1-wiiu-hello: start, core %d, auto-exit %d s", OSGetCoreId(), autoExitSeconds);

    mkdir(OUT_DIR, 0777);
    FILE *f = fopen(OUT_FILE, "w");
    if (f) {
        OSCalendarTime t;
        OSTicksToCalendarTime(OSGetTime(), &t);
        fprintf(f, "rac1-wiiu-hello %04d-%02d-%02d %02d:%02d:%02d, big-endian=%d\n",
                t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
                (int)(*(const unsigned char *)&(unsigned){1} == 0));
        fclose(f);
        WHBLogPrintf("rac1-wiiu-hello: wrote " OUT_FILE);
    } else {
        WHBLogPrintf("rac1-wiiu-hello: fopen failed for " OUT_FILE);
    }
    WHBLogPrintf("rac1-wiiu-hello: done");
    if (autoExitSeconds > 0)
        WHBLogPrintf("Back to the menu in %d s", autoExitSeconds);
    else
        WHBLogPrintf("Press HOME to exit");

    OSTime start = OSGetTime();
    int exitRequested = 0;
    int frame = 0;
    while (WHBProcIsRunning()) {
        if (!exitRequested && autoExitSeconds > 0 &&
            OSTicksToSeconds(OSGetTime() - start) >= autoExitSeconds) {
            WHBLogPrintf("rac1-wiiu-hello: auto-exit after %d frames", frame);
            exitRequested = 1;
            SYSLaunchMenu();
        }
        WHBLogConsoleDraw();  // does nothing while the foreground is released
        OSSleepTicks(OSMillisecondsToTicks(16));
        frame++;
    }

    WHBLogPrintf("rac1-wiiu-hello: exit");
    WHBLogConsoleFree();
    WHBLogUdpDeinit();
    WHBLogModuleDeinit();
    WHBProcShutdown();
    return 0;
}
