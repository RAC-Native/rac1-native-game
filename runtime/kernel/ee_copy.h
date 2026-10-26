#ifndef RAC1_RUNTIME_KERNEL_EE_COPY_H
#define RAC1_RUNTIME_KERNEL_EE_COPY_H
// A copy of parts of EE memory, read in place of the live memory by the threads that ask for it.
//
// The host draws a pipelined frame after (with the game thread: during) the next game frame's update. What that drawing reads
// of the game's memory (the modern lighting: the lights, the light sets, the mobys; the ambient particles) is copied when the
// frame is prepared (ee_copy_take, the main thread, the game's thread not running) and read from the copy while the frame is
// drawn (ee_copy_use on the drawing thread; ee_copy_read .. in the readers). An address outside the copy is read live;
// RAC1_DRAW_MEM_CHECK=1 reports those reads (the audit of what the drawing reads beside the update).
//
// Host only: the game never reads the copy.
#include <runtime/kernel/ee_memory.h>

#include <cstdint>
#include <cstring>

namespace rac1 {

struct EeCopyRange {
    uint32_t first = 0, end = 0;  // [first, end)
};
// orig: (host) the ranges copied now (up to ee_copy_detail::kRanges; empty ones skipped)
void ee_copy_take(const EeCopyRange* ranges, int n);
// orig: (host) the calling thread reads the copy from now (`on`) or the live memory
void ee_copy_use(bool on);

namespace ee_copy_detail {
struct Range {
    uint32_t first = 0, end = 0;
    const uint8_t* bytes = nullptr;
};
constexpr int kRanges = 4;
extern Range g_ranges[kRanges];
extern thread_local bool t_use;
// orig: (host) a read outside the copy while it is in use (RAC1_DRAW_MEM_CHECK)
void live_read(uint32_t a, uint32_t size);
}  // namespace ee_copy_detail

// orig: (host) a read of EE memory from the copy when the calling thread uses it and the address is inside it, else live
template <typename T>
T ee_copy_read(uint32_t a) {
    using namespace ee_copy_detail;
    if (t_use) {
        for (const Range& r : g_ranges)
            if (a - r.first < r.end - r.first && r.end - a >= sizeof(T)) {
                T v;
                std::memcpy(&v, r.bytes + (a - r.first), sizeof(T));
                return v;
            }
        live_read(a, sizeof(T));
    }
    return ee_read<T>(a);
}
inline int32_t draw_lw(uint32_t a) { return ee_copy_read<int32_t>(a); }
inline uint32_t draw_lwu(uint32_t a) { return ee_copy_read<uint32_t>(a); }
inline int16_t draw_lh(uint32_t a) { return ee_copy_read<int16_t>(a); }
inline uint16_t draw_lhu(uint32_t a) { return ee_copy_read<uint16_t>(a); }
inline int8_t draw_lb(uint32_t a) { return ee_copy_read<int8_t>(a); }
inline uint8_t draw_lbu(uint32_t a) { return ee_copy_read<uint8_t>(a); }
inline float draw_lwc1(uint32_t a) { return ee_copy_read<float>(a); }

}  // namespace rac1

#endif
