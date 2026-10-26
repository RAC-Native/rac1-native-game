// Wii U smoke test: logs over UDP (Aroma, port 4405) and writes a file to the SD.
// It returns to the Wii U Menu by itself after a few seconds (argv[1], default 5;
// 0 = wait for HOME), so the console is always ready for the next wiiload.
#include <coreinit/core.h>
#include <coreinit/time.h>
#include <coreinit/memheap.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <sysapp/launch.h>
#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_udp.h>
#include <whb/log_module.h>
#include <whb/log_cafe.h>
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
    WHBLogCafeInit();

    int autoExitSeconds = argc > 1 ? atoi(argv[1]) : 5;
    WHBLogPrintf("rac1-wiiu-hello: start, core %d, auto-exit %d s", OSGetCoreId(), autoExitSeconds);

    // Draw something, otherwise the Wii U Menu's launch splash stays on screen.
    OSScreenInit();
    uint32_t tvSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    void *tvBuf = MEMAllocFromDefaultHeapEx(tvSize, 0x100);
    void *drcBuf = MEMAllocFromDefaultHeapEx(drcSize, 0x100);
    OSScreenSetBufferEx(SCREEN_TV, tvBuf);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuf);
    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);

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
    OSTime start = OSGetTime();
    int exitRequested = 0;
    for (int frame = 0; WHBProcIsRunning(); frame++) {
        if (exitRequested) {
            // The foreground is being released: no more drawing, just let ProcUI finish.
            OSSleepTicks(OSMillisecondsToTicks(16));
            continue;
        }
        int elapsed = (int)OSTicksToSeconds(OSGetTime() - start);
        if (autoExitSeconds > 0 && elapsed >= autoExitSeconds) {
            WHBLogPrintf("rac1-wiiu-hello: auto-exit after %d frames", frame);
            exitRequested = 1;
            SYSLaunchMenu();
            continue;
        }
        for (int s = 0; s < 2; s++) {
            OSScreenID id = s ? SCREEN_DRC : SCREEN_TV;
            OSScreenClearBufferEx(id, 0x10203000);
            OSScreenPutFontEx(id, 0, 0, "rac1-native Wii U hello");
            OSScreenPutFontEx(id, 0, 2, f ? "SD: wrote " OUT_FILE : "SD: fopen FAILED");
            char line[64];
            if (autoExitSeconds > 0)
                snprintf(line, sizeof line, "Back to the menu in %d s", autoExitSeconds - elapsed);
            else
                snprintf(line, sizeof line, "Press HOME to exit");
            OSScreenPutFontEx(id, 0, 4, line);
            snprintf(line, sizeof line, "frame %d", frame);
            OSScreenPutFontEx(id, 0, 6, line);
        }
        DCFlushRange(tvBuf, tvSize);
        DCFlushRange(drcBuf, drcSize);
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
        OSSleepTicks(OSMillisecondsToTicks(16));
    }

    WHBLogPrintf("rac1-wiiu-hello: exit");
    OSScreenShutdown();
    MEMFreeToDefaultHeap(tvBuf);
    MEMFreeToDefaultHeap(drcBuf);
    WHBLogCafeDeinit();
    WHBLogModuleDeinit();
    WHBLogUdpDeinit();
    WHBProcShutdown();
    return 0;
}
