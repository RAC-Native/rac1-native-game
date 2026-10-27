// (experiment, RAC1_WIIU_EXACT_SQRT=ON: -Wl,--wrap=sqrtf) sqrtf done on integers, correctly rounded to nearest-even, in
// place of newlib's (which starts from the 750's frsqrte estimate): to tell an emulator's estimate table (Cemu's) from a
// real difference. Round to nearest only: never for a console run in the EE's rounding mode.
#include <cstdint>
#include <cstring>

extern "C" float __wrap_sqrtf(float x) {
    uint32_t u;
    std::memcpy(&u, &x, 4);
    const uint32_t mag = u & 0x7fffffffu;
    if (mag == 0 || mag >= 0x7f800000u) {  // (+-0, inf, NaN as they are; a negative number below)
        if (mag > 0x7f800000u || (u >> 31) == 0 || mag == 0) return x;
    }
    if ((u >> 31) != 0) {
        const uint32_t nan = 0x7fc00000u;
        float f;
        std::memcpy(&f, &nan, 4);
        return f;
    }
    int32_t e = static_cast<int32_t>(u >> 23);
    uint32_t m = u & 0x7fffffu;
    if (e == 0) {  // (a denormal: normalized)
        while ((m & 0x800000u) == 0) {
            m <<= 1;
            --e;
        }
        ++e;
    } else {
        m |= 0x800000u;
    }
    int32_t ue = e - 127;
    if ((ue & 1) != 0) {
        m <<= 1;
        --ue;
    }
    const uint64_t big = static_cast<uint64_t>(m) << 25;  // sqrt(x) = sqrt(big) * 2^((ue - 48) / 2)
    uint64_t r = 0, rem = 0;
    for (int i = 62; i >= 0; i -= 2) {  // (the integer square root, two bits at a time)
        rem = rem << 2 | ((big >> i) & 3u);
        r <<= 1;
        const uint64_t t = (r << 1) | 1u;
        if (rem >= t) {
            rem -= t;
            r |= 1u;
        }
    }
    uint32_t f = static_cast<uint32_t>(r >> 1);
    const bool half = (r & 1u) != 0, sticky = rem != 0;
    if (half && (sticky || (f & 1u) != 0)) ++f;
    int32_t ex = (ue - 48) / 2 + 1 + 23 + 127;
    if (f == 0x1000000u) {
        f >>= 1;
        ++ex;
    }
    const uint32_t bits = static_cast<uint32_t>(ex) << 23 | (f & 0x7fffffu);
    float out;
    std::memcpy(&out, &bits, 4);
    return out;
}
