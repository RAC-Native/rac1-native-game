// The VU1 side of the VIF1 path (hardware layer): the VIF1 registers (cycle, mode / mask,
// row / column, BASE / OFFSET / TOPS double buffering, ITOP), its commands that feed VU1 (UNPACK
// into the 16 KB data memory, MPG into the 16 KB micro memory) and the microprogram starts
// (MSCAL / MSCALF / MSCNT). A started program runs as the ported C++ program registered for the
// code then in micro memory (src/game ports, proved against the VU interpreter running the
// original); with RAC1_VU1_INTERP set, or for code with no port, the VU interpreter runs the
// uploaded code itself (ADR 0005: the coprocessor backend, a debug / reference path) -- by
// default an unported program is skipped and logged once. XGKICK hands the GIF packet at a
// data memory address to a sink (the GS, path 1).
#ifndef RAC1_RUNTIME_VU_VU1_H
#define RAC1_RUNTIME_VU_VU1_H

#include <runtime/vu/vu_interpreter.h>

#include <array>
#include <cstdint>
#include <functional>

namespace rac1::vu1 {

// A GIF packet out of VU1 memory: `qwc` quadwords (wrapped out of the 16 KB memory, contiguous).
using KickSink = std::function<void(const uint8_t* data, uint32_t qwc)>;
// A ported microprogram: runs from instruction `start` on the VU1 registers / memory of `vu`
// until its end (E bit), calling `kick(qword address)` for each XGKICK.
using ProgramFn = void (*)(VUState& vu, uint32_t start, const std::function<void(uint32_t)>& kick);

struct Vu1 {
    VUState st{};                                  // registers and the 16 KB data memory
    std::array<uint64_t, 2048> micro{};            // the 16 KB micro memory
    uint32_t cl = 1, wl = 1, mode = 0, mask = 0;   // STCYCL, STMOD, STMASK
    uint32_t row[4] = {}, col[4] = {};             // STROW, STCOL
    uint32_t base = 0, ofst = 0, tops = 0, top = 0, itops = 0, itop = 0;
    bool dbf = false;
    uint32_t next_pc = 0;                          // where MSCNT resumes
};

Vu1& vu1_global();
// Testing: run every program on the interpreter; see every kicked packet.
extern bool g_force_interpreter;
extern thread_local uint32_t g_last_start;  // the micro address of the program started last on this thread (testing)
// (host) the widening of the view the chain being walked on this thread is drawn into (gs::Gs::set_side_widen: 1 = the game's own
// 4:3 frustum): the ported programs' tests that drop a primitive lying wholly past the frustum's sides use it, so that what the wider
// window shows is not dropped (their clipping, at the guard band, is unchanged). x, y.
extern thread_local float g_side_widen[2];
// (host) an observer of the ported programs' starts on this thread (the GS replica's walk sets one while it walks a chain:
// the MRP's native drawing of VU1 program 7's surfaces takes their input before the program transforms it in place). `before` runs
// before the program (its hook included), `after` once it has ended.
struct StartObserver {
    virtual ~StartObserver() = default;
    virtual void before(Vu1& vu, uint32_t start, uint64_t hash) = 0;
    virtual void after(Vu1& vu, uint32_t start, uint64_t hash) = 0;
};
extern thread_local StartObserver* g_start_observer;
extern KickSink g_kick_tap;
using StartHook = std::function<bool(Vu1& vu, uint32_t start, uint64_t hash, ProgramFn fn, const std::function<void(uint32_t)>& kick)>;
extern StartHook g_start_hook;

// Registers the port of the microprogram whose `n` instructions, loaded at micro address `addr`,
// hash (fnv1a64 of their little-endian 64-bit words) to `hash`.
void register_program(uint32_t addr, uint32_t n, uint64_t hash, ProgramFn fn, const char* name);
uint64_t code_hash(const uint64_t* words, uint32_t n);
// Testing (RAC1_VU_FALLBACK=1): the program starts no port ran (skipped, or on the interpreter with RAC1_VU1_INTERP), per start and micro image (stderr).
void fallback_report();

// The VIF1 commands that concern VU1. `code` is the VIF code, `data` its data (size known to the
// caller: vif_data_bytes). Returns false for commands that are not VU1's (DIRECT, ...).
bool vif1_command(Vu1& vu, uint32_t code, const uint8_t* data, const KickSink& kick);
// The bytes of data that follow VIF code `code` with the current cycle setting (0: none).
uint32_t vif_data_bytes(const Vu1& vu, uint32_t code);

// Starts the program at instruction `start` (MSCAL's address / 8) with the double buffer swap.
void start_program(Vu1& vu, uint32_t start, const KickSink& kick);
// Runs the program at `start` with the VU interpreter on the uploaded micro memory (the
// reference the ports are proved against).
void run_interpreter(Vu1& vu, uint32_t start, const std::function<void(uint32_t)>& kick);
// The length in quadwords of the GIF packet (up to its EOP) at quadword `addr` of VU memory.
uint32_t gif_packet_qwc(const VUState& st, uint32_t addr);

}  // namespace rac1::vu1

#endif
