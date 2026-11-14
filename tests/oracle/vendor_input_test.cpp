#include <game/ovl/vendor_screens.h>
#include <game/ovl/region_core.h>
#include <runtime/kernel/ee_memory.h>
#include <cstring>
#include <cstdio>

// Disc-backed regression for #172: invoke from each prepared asset root with its expected build.
// Calls the same helpers used by screen start/update and All Ammo; no duplicate input implementation.
int main(int argc, char** argv) {
    using namespace rac1;
    if (argc != 2 || (std::strcmp(argv[1], "--1.02") != 0 && std::strcmp(argv[1], "--pre-1.02") != 0)) {
        std::fprintf(stderr, "Usage: vendor_input_test --1.02|--pre-1.02 (from the prepared disc's asset root)\n");
        return 2;
    }
    init_memory();
    const bool later = region_core_moved();
    if (later != (std::strcmp(argv[1], "--1.02") == 0)) {
        std::fprintf(stderr, "Wrong disc/maps for the requested build\n");
        shutdown_memory();
        return 1;
    }
    int failures = 0;
    auto check = [&](bool ok, const char* what, int level) {
        if (!ok) { std::printf("FAIL level %d: %s\n", level, what); ++failures; }
    };
    for (int level = 0; level < 19; ++level) {
        ovl_reloc_set_level(level);
        const uint32_t q = vendor::QUANTITY;
        const uint32_t history = vendor::PAD_BEFORE;
        check((q == history) == later, "disc maps must exercise the expected history layout", level);
        // Cross's bit deliberately overlaps a plausible quantity (64). On PAL 2.00 this used to suppress Buy All.
        ee_write<uint32_t>(q, 64);
        if (!later) ee_write<uint32_t>(history, 0);
        ee_write<uint32_t>(vendor::PAD_NOW, vendor::BTN_CROSS);
        check(vendor::button_pressed(vendor::BTN_CROSS), "fresh Cross", level);
        vendor::pad_history_set(vendor::BTN_RIGHT);
        check(ee_read<uint32_t>(q) == 64, "Right history must preserve quantity", level);
        vendor::pad_history_set(0);
        check(ee_read<uint32_t>(q) == 64, "release/reset must preserve quantity", level);
        ee_write<uint32_t>(vendor::PAD_NOW, 0);
        check(!vendor::button_pressed(vendor::BTN_CROSS), "released Cross", level);
        if (!later) {
            vendor::pad_history_set(vendor::BTN_CROSS);
            ee_write<uint32_t>(vendor::PAD_NOW, vendor::BTN_CROSS);
            check(!vendor::button_pressed(vendor::BTN_CROSS), "old build retains history guard", level);
        }
    }
    std::printf("vendor input: %s, 19 levels, %d failures\n", later ? "1.02" : "pre-1.02", failures);
    shutdown_memory();
    return failures ? 1 : 0;
}
