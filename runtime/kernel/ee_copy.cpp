// A copy of parts of EE memory read in place of the live memory: see ee_copy.h.
#include <runtime/kernel/ee_copy.h>

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <vector>

namespace rac1 {

namespace ee_copy_detail {
Range g_ranges[kRanges];
thread_local bool t_use = false;

namespace {
std::vector<uint8_t> g_bytes[kRanges];
const bool g_check = [] {
    const char* e = std::getenv("RAC1_DRAW_MEM_CHECK");
    return e != nullptr && std::atoi(e) != 0;
}();
std::mutex g_check_m;
std::set<uint32_t> g_check_seen;
}  // namespace

// orig: (host) see ee_copy.h
void live_read(uint32_t a, uint32_t size) {
    if (!g_check) return;
    std::lock_guard<std::mutex> lk(g_check_m);
    if (g_check_seen.size() < 256 && g_check_seen.insert(a & ~0xfu).second)
        std::fprintf(stderr, "[draw-mem] a live read of %u bytes at %08x while a pipelined frame is drawn\n", size, a);
}
}  // namespace ee_copy_detail

// orig: (host) see ee_copy.h
void ee_copy_take(const EeCopyRange* ranges, int n) {
    using namespace ee_copy_detail;
    const uint8_t* ram = ee_get_ram_base();
    for (int i = 0; i < kRanges; ++i) {
        const bool ok = ram != nullptr && i < n && ranges[i].first != 0 && ranges[i].end > ranges[i].first && ranges[i].end <= 0x02000000u;
        if (!ok) {
            g_ranges[i] = Range{};
            continue;
        }
        g_bytes[i].assign(ram + ranges[i].first, ram + ranges[i].end);
        g_ranges[i] = Range{ranges[i].first, ranges[i].end, g_bytes[i].data()};
    }
}

// orig: (host) see ee_copy.h
void ee_copy_use(bool on) { ee_copy_detail::t_use = on; }

}  // namespace rac1
