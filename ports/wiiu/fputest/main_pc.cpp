// The PC side of the float check: the same hashes as the Wii U's (ports/wiiu/fputest/main_wiiu.cpp).
#include "fputest_core.h"
#include "denorm_cases.h"

#include <cstdio>

int main() {
    rac1::ps2fpu::enter_ee_rounding();
    const auto r = fputest::run(1 << 20);
    std::printf("add %08x mul %08x div %08x sqrt %08x denorm %08x vendor %08x\n", r.add, r.mul, r.div, r.sqrt,
                r.denorm, r.vendor);
    uint32_t direct = 0;
    const uint32_t co = fputest::coroutine_check(1000, &direct);
    std::printf("coroutine %08x direct %08x %s\n", co, direct, co == direct ? "same" : "DIFFERENT");
    fputest::denorm_report([](const char* l) { std::printf("%s\n", l); });
    return 0;
}
