// (testing, 2026-10-29: the frame's GX2 work counted - AGENT_PROMPT_PERF.md step 1) the backends' GX2DrawDone / GX2Invalidate /
// draws / cache flushes counted per host frame, the GX2DrawDone calls by their call site (file, line): wiiu_gfx.cpp's "[gx2]" line
// every timing window, and the GamePad debug menu's timing page. Included by the backends after their GX2 headers: the macros below
// route their calls through the counters. A counter's cost is an increment (the draw thread's; the world's worker flushes too: the
// counts are approximate there, never wrong by more than a frame's).
#pragma once

#include <coreinit/cache.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/mem.h>

#include <cstdint>

namespace rac1::gx2c {

struct Counters {
    uint32_t draw_done = 0;    // GX2DrawDone (each one: the CPU waits for the GPU to drain)
    uint32_t invalidate = 0;   // GX2Invalidate (a SURFACE_SYNC on the GPU each)
    uint32_t draws = 0;        // GX2DrawEx / GX2DrawIndexedEx
    uint32_t flushes = 0;      // DCFlushRange
    uint64_t flush_bytes = 0;  // their bytes: what the CPU wrote for the GPU this frame, about
};
extern Counters g;

// a GX2DrawDone's call site, counted (a small table: the first 32 sites)
void note_draw_done(const char* file, int line);

inline BOOL draw_done(const char* file, int line) {
    ++g.draw_done;
    note_draw_done(file, line);
    return ::GX2DrawDone();
}
inline void invalidate(GX2InvalidateMode mode, void* buffer, uint32_t size) {
    ++g.invalidate;
    ::GX2Invalidate(mode, buffer, size);
}
inline void draw_ex(GX2PrimitiveMode mode, uint32_t count, uint32_t offset, uint32_t instances) {
    ++g.draws;
    ::GX2DrawEx(mode, count, offset, instances);
}
inline void draw_indexed_ex(GX2PrimitiveMode mode, uint32_t count, GX2IndexType type, const void* indices, uint32_t offset, uint32_t instances) {
    ++g.draws;
    ::GX2DrawIndexedEx(mode, count, type, indices, offset, instances);
}
inline void flush_range(void* p, uint32_t size) {
    ++g.flushes;
    g.flush_bytes += size;
    ::DCFlushRange(p, size);
}

}  // namespace rac1::gx2c

#define GX2DrawDone() rac1::gx2c::draw_done(__FILE__, __LINE__)
#define GX2Invalidate(m, b, s) rac1::gx2c::invalidate((m), (b), (s))
#define GX2DrawEx(m, c, o, i) rac1::gx2c::draw_ex((m), (c), (o), (i))
#define GX2DrawIndexedEx(m, c, t, p, o, i) rac1::gx2c::draw_indexed_ex((m), (c), (t), (p), (o), (i))
#define DCFlushRange(p, s) rac1::gx2c::flush_range((void*)(p), (uint32_t)(s))
