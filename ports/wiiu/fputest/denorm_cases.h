// Denormal cases, one at a time: each prints its operands and results, so the PC and the console can be
// compared case by case (fputest's "denorm" hash differs between them).
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace fputest {
inline float fb(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
inline uint32_t bf(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
struct DenormCase { const char* what; uint32_t a, b; };
inline const DenormCase k_denorm_cases[] = {
    {"den+0", 0x00000001u, 0x00000000u},
    {"den+den", 0x00400000u, 0x00400000u},
    {"den+min", 0x00000001u, 0x00800000u},
    {"-den+min", 0x80000001u, 0x00800000u},
    {"den+1", 0x00400000u, 0x3f800000u},
    {"-den+1", 0x80400000u, 0x3f800000u},
    {"den*1", 0x00400000u, 0x3f800000u},
    {"den*2^30", 0x00400000u, 0x4e800000u},
    {"min*0.5", 0x00800000u, 0x3f000000u},
    {"min*0.99", 0x00800000u, 0x3f7d70a4u},
    {"1.5min*0.75", 0x00c00000u, 0x3f400000u},
    {"min-1ulp", 0x00800001u, 0x80800000u},
    {"big*den", 0x7f000000u, 0x00000002u},
    {"den-den", 0x00400001u, 0x80400000u},
};
// op 0: a + b, op 1: a * b, op 2: a - b
inline void denorm_report(void (*out)(const char*)) {
    char line[128];
    for (const auto& c : k_denorm_cases) {
        volatile float a = fb(c.a), b = fb(c.b);
        const float s = a + b, m = a * b, d = a - b;
        std::snprintf(line, sizeof line, "%-12s a %08x b %08x  + %08x  * %08x  - %08x", c.what, c.a, c.b, bf(s), bf(m),
                      bf(d));
        out(line);
    }
}
}  // namespace fputest
