// tests/gpu/gpu_name_test.cpp — the GPU row's short names (src/platform/gpu_select.h) on real-world Vulkan device strings.
//   cmake --build build --target gpu_name_test && ./build/gpu_name_test
#include <platform/gpu_select.h>

#include <cstdio>

namespace {
int g_failed = 0;

// orig: (none) — test: one name
void check(const char* full, const char* want) {
    const std::string got = rac1::gpu_short_name(full);
    const bool ok = got == want;
    std::printf("%s \"%s\" -> \"%s\"%s\n", ok ? "ok  " : "FAIL", full, got.c_str(), ok ? "" : (std::string(" (wanted \"") + want + "\")").c_str());
    if (!ok) ++g_failed;
}
}  // namespace

// orig: (none) — test
int main() {
    check("NVIDIA GeForce RTX 4070 Ti", "RTX 4070 Ti");
    check("NVIDIA GeForce RTX 3060 Laptop GPU", "RTX 3060 Laptop GPU");
    check("AMD Radeon 890M (RADV GFX1150)", "Radeon 890M");
    check("AMD Radeon RX 7900 XTX", "Radeon RX 7900 XTX");
    check("AMD Radeon(TM) Graphics (RADV RAPHAEL_MENDOCINO)", "Radeon Graphics");
    check("Intel(R) Arc(TM) A770 Graphics", "Arc A770");
    check("Intel(R) UHD Graphics 620 (CML GT2)", "UHD Graphics 620");
    check("Intel(R) Iris(R) Xe Graphics", "Iris Xe");
    check("Apple M2 Pro", "M2 Pro");
    check("llvmpipe (LLVM 19.1.7, 256 bits)", "llvmpipe");
    check("Microsoft Direct3D12 (NVIDIA GeForce RTX 3060)", "RTX 3060");
    check("Qualcomm Adreno (TM) 740", "Adreno 740");
    check("Mali-G78", "Mali-G78");
    check("NVIDIA Corporation", "NVIDIA");
    check("  AMD   ", "AMD");
    check("", "");
    const std::string clipped = rac1::gpu_clip("Radeon RX 7900 XTX Special", 18);
    std::printf("%s clip -> \"%s\"\n", clipped == "Radeon RX 7900..." ? "ok  " : "FAIL", clipped.c_str());
    if (clipped != "Radeon RX 7900...") ++g_failed;
    std::printf(g_failed == 0 ? "all ok\n" : "%d failed\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
