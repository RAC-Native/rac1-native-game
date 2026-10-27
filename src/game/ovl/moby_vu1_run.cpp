// The moby pass's VU1 programs on the port (split from the generated moby_vu1.cpp): a started program run to its end on
// the generated port (moby_vu1_fast, or the exact moby_vu1_step with RAC1_VU_EXACT=1), its XGKICKs to the backend; the self-check
// (RAC1_VU_SELFCHECK=1) and the registration with the VU1 backend. Hand-written.
#include <game/ovl/moby_vu1.h>

#include <runtime/vu/vu1.h>
#include <runtime/vu/vu_native.h>
#include <runtime/vu/vu_prof.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rac1 {

// orig: (none) a program run from `start` to its end (E bit) or its instruction budget; the instruction count
static uint64_t moby_vu1_run(VUState& vu, uint32_t start, VuNativeStep step) {
    VuMicroRun run;  // (execute_microprogram, with its instruction count for RAC1_VU_PROF)
    run.start(vu, nullptr, 2048, start, step);
    run.limit_issue(2000000);
    while (run.pipe().issued < 2000000 && run.step()) {
    }
    run.finish();
    return run.pipe().issued;
}

// orig: (none) testing (RAC1_VU_SELFCHECK=1): each program run on a copy by the exact port as well, the end state and
// the XGKICK packets compared with the fast port's (what differs goes to stderr)
static uint64_t moby_vu1_run_checked(VUState& vu, uint32_t start, const std::function<void(uint32_t)>& kick) {
    using Packets = std::vector<std::vector<uint8_t>>;
    auto tap = [](VUState& st, Packets& out, uint32_t addr) {
        const uint32_t qwc = vu1::gif_packet_qwc(st, addr);
        std::vector<uint8_t> pkt(static_cast<size_t>(qwc) * 16 + 4);
        std::memcpy(pkt.data(), &addr, 4);
        for (uint32_t q = 0; q < qwc; ++q) std::memcpy(&pkt[4 + q * 16], &st.mem[((addr + q) & 0x3ff) << 4], 16);
        out.push_back(std::move(pkt));
    };
    auto ref = std::make_unique<VUState>(vu);
    Packets pr, pf;
    ref->xgkick_hook = [&](uint32_t addr) { tap(*ref, pr, addr); };
    vu.xgkick_hook = [&](uint32_t addr) {
        tap(vu, pf, addr);
        kick(addr);
    };
    const uint64_t ir = moby_vu1_run(*ref, start, &moby_vu1_step);
    const uint64_t i = moby_vu1_run(vu, start, &moby_vu1_fast);
    static int shown = 0;
    std::string d;
    if (ir != i) d += "issued ";
    if (std::memcmp(ref->vf.data(), vu.vf.data(), sizeof(ref->vf)) != 0) d += "vf ";
    if (ref->vi != vu.vi) d += "vi ";
    if (std::memcmp(&ref->acc, &vu.acc, 16) != 0) d += "acc ";
    if (std::memcmp(&ref->q, &vu.q, 4) != 0 || std::memcmp(&ref->i, &vu.i, 4) != 0) d += "q/i ";
    if (ref->macflag != vu.macflag || ref->statusflag != vu.statusflag || ref->clipflag != vu.clipflag) d += "flags ";
    if (ref->mem != vu.mem) d += "mem ";
    if (pr != pf) d += "kicks ";
    if (!d.empty() && shown++ < 20)
        std::fprintf(stderr, "[vu1 selfcheck] start %03x: the fast port differs from the exact one (%llu / %llu instructions): %s\n", start,
                     static_cast<unsigned long long>(ir), static_cast<unsigned long long>(i), d.c_str());
    static int checked = 0;
    if (++checked % 5000 == 0) std::fprintf(stderr, "[vu1 selfcheck] %d programs checked\n", checked);
    return i;
}

// orig: (none) a started program run to its end (E bit) on the port, its XGKICKs to the backend
static void moby_vu1(VUState& vu, uint32_t start, const std::function<void(uint32_t)>& kick) {
    static const bool like_wiiu = std::getenv("RAC1_VU1_LIKE_WIIU") != nullptr;  // testing: as the Wii U's build, which has no such program
    if (like_wiiu) return;
    static const bool exact = std::getenv("RAC1_VU_EXACT") != nullptr || std::getenv("RAC1_VU1_EXACT") != nullptr;  // testing: the exact step-by-step port
    static const bool check = std::getenv("RAC1_VU_SELFCHECK") != nullptr;
    vuprof::Scope prof(1);
    if (check && !exact) {
        prof.insns = moby_vu1_run_checked(vu, start, kick);
    } else {
        if (vuprof::enabled()) {  // (the XGKICK sink's time counted apart: the GS emulation behind it is not the micro program)
            vu.xgkick_hook = [&kick](uint32_t addr) {
                vuprof::Scope sink(2);
                kick(addr);
            };
        } else {
            vu.xgkick_hook = kick;
        }
        prof.insns = moby_vu1_run(vu, start, exact ? &moby_vu1_step : &moby_vu1_fast);
    }
    vu.xgkick_hook = nullptr;
}

// orig: (none) the port's registration with the VU1 backend (fnv1a64 of the 0x508 instructions)
void moby_vu1_register() {
    vu1::register_program(0x000, 0x508, 0x1585563a86921802ull, &moby_vu1, "mobys");
}

}  // namespace rac1
