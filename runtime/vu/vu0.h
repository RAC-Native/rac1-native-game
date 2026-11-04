// VU0 (hardware layer): its registers and 4 KB data memory -- the one register file the EE's
// macro-mode (COP2) code and the micro programs share --, its 4 KB micro memory filled by VIF0
// (MPG), and the micro programs started by VCALLMS / VCALLMSR beside the EE. The ported EE code
// runs a started program up to the point where it waits on the EE (a busy-wait on its integer
// registers: the EE's CTC2 releases it) or ends; the program is the uploaded code on the VU
// interpreter (ADR 0005) until a port is registered for it.
#ifndef RAC1_RUNTIME_VU_VU0_H
#define RAC1_RUNTIME_VU_VU0_H

#include <runtime/kernel/hot_inline.h>
#include <runtime/vu/vu_interpreter.h>

#include <array>
#include <cstdint>

namespace rac1::vu0 {

struct Vu0 {
    VUState st{};
    std::array<uint64_t, 512> micro{};
    VuMicroRun run;
    uint32_t cmsar0 = 0;
};

// (inline: the macro-mode helpers reach it for every operand)
namespace detail {
extern Vu0 g_instance;
}  // namespace detail
inline Vu0& global() { return detail::g_instance; }
// VIF0 DMA in source chain mode from `tadr` (tte: the tags carry VIF codes): MPG into the micro
// memory; NOP / STCYCL / FLUSH* without effect here.
void vif0_send_chain(uint32_t tadr, bool tte);
// VCALLMS / VCALLMSR: a program still running goes to its end first, then this one starts at
// micro address `addr` (and runs until it waits or ends).
void call(uint32_t addr);
// The running program up to its next wait on the EE or its end.
void run_until_blocked();
// The EE waits for the program's end (interlocked transfers, macro ops while it runs).
void drain();
#if defined(RAC1_INLINE_HELPERS) && RAC1_INLINE_HELPERS
inline bool running() { return global().run.running(); }  // (every macro op asks: inline, a load and a test)
#else
bool running();
#endif
// Testing (RAC1_VU0_CHAIN=1): the hash of the VU0 state at the points where the program stopped (see vu0.cpp).
uint64_t chain_hash();
// Testing (RAC1_VU_FALLBACK=1): the programs that ran on the interpreter (no port for their code), per start and micro image (stderr).
void fallback_report();
// Registers the port of the micro program whose n instructions at micro address addr hash
// (fnv1a64 of their little-endian words) to hash: a program started inside that range while the
// code there matches runs as the port (runtime/vu/vu_native.h), else on the interpreter.
void register_program(uint32_t addr, uint32_t n, uint64_t hash, VuNativeStep step, const char* name);

}  // namespace rac1::vu0

#endif
