#ifndef RAC1_RUNTIME_EE_INTERPRETER_H
#define RAC1_RUNTIME_EE_INTERPRETER_H

// R5900 (PS2 Emotion Engine) interpreter over the runtime's EE memory.
//
// Purpose (ADR 0006): an *oracle* that executes the original game code from EE
// RAM, so every C++ port under src/game/ can be checked against the real
// function (same inputs -> same memory writes / return values), and a
// temporary *bridge* that lets a ported function call original callees that
// are not ported yet. It is not a PS2 emulator: no timing, no interrupts, no
// DMA/GS/IOP. Hardware registers read as 0 and writes are dropped.

#include <runtime/vu/vu_interpreter.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rac1 {

struct EeCpu {
    // General purpose registers are 128-bit on the R5900.
    u128 gpr[32]{};
    u128 hi{}, lo{};          // hi/lo hold HI0/LO0 in .lo and HI1/LO1 in .hi
    uint64_t sa = 0;          // shift amount register
    float fpr[32]{};          // COP1
    float acc = 0.0f;         // COP1 accumulator
    bool fcr_c = false;       // COP1 condition bit (FCR31 bit 23)
    uint32_t pc = 0;
    VUState* vu0 = nullptr;   // COP2 macro mode state (owned elsewhere, see EeInterpreter)

    uint32_t r32(int r) const { return static_cast<uint32_t>(gpr[r].lo); }
    int32_t s32(int r) const { return static_cast<int32_t>(gpr[r].lo); }
    void set32(int r, int32_t v) {  // sign-extend to 64, keep upper 64 untouched like the hardware
        if (r != 0) gpr[r].lo = static_cast<uint64_t>(static_cast<int64_t>(v));
    }
    void set64(int r, uint64_t v) { if (r != 0) gpr[r].lo = v; }
};

// The VU0 micro memory filled from a VIF0 source chain at tadr (tte: the tags carry VIF codes),
// as the game's VIF0 DMA kicks do (the oracle sets up states with it).
void ee_vif0_chain(uint32_t tadr, bool tte);

class EeInterpreter {
public:
    using Hook = std::function<void(EeCpu&)>;

    EeInterpreter();

    // Intercept a function entry: when the PC reaches `addr`, the hook runs
    // instead (it must set return registers itself); execution resumes at $ra.
    void set_hook(uint32_t addr, Hook hook) { hooks_[addr] = std::move(hook); }
    void clear_hooks() { hooks_.clear(); }
    void remove_hook(uint32_t addr) { hooks_.erase(addr); }
    // A conditional hook: it runs at the function's entry and returns whether it handled the call (the
    // function returns to $ra as with set_hook); false: the original code runs (the oracle's stand-ins for
    // the calls a port leaves TODO_PORT in one context only).
    using CondHook = std::function<bool(EeCpu&)>;
    void set_cond_hook(uint32_t addr, CondHook hook) { cond_hooks_[addr] = std::move(hook); }
    void remove_cond_hook(uint32_t addr) { cond_hooks_.erase(addr); }
    // A probe: when the PC reaches `addr`, `probe` runs and the instruction there then executes as usual (nothing is
    // replaced; run before the hooks above). A delay slot's address is never reached this way (it runs in its branch's
    // step). The oracle's HUD-width check records the hooks.glue points of a function with them (tests/oracle, H1).
    void set_probe(uint32_t addr, Hook probe) { probes_[addr] = std::move(probe); }
    void clear_probes() { probes_.clear(); }

    // Call the original function at `addr` with $a0..$a3 = args, $gp = gp,
    // $sp = sp (a private stack is used when 0). Returns false on an
    // unsupported instruction or when `max_instructions` is exceeded.
    bool call(uint32_t addr, std::initializer_list<uint32_t> args = {}, uint32_t gp = 0x00166c00,
              uint64_t max_instructions = 50'000'000);

    EeCpu& cpu() { return cpu_; }
    VUState& vu0() { return vu0_; }
    // VU0 micro programs run beside the EE (num VU0 instructions per den EE instructions) instead
    // of to their end at the VCALLMS: for the routines that hand data to a running program.
    void set_vu0_coexec(bool on, int num = 1, int den = 1) { vu0_coexec_ = on; vu0_num_ = num; vu0_den_ = den; }
    const std::string& error() const { return error_; }
    uint64_t instructions_executed() const { return executed_; }
    // Last executed PCs (ring buffer, most recent last) — for debugging hangs.
    std::vector<uint32_t> recent_pcs() const;
    // Shadow call stack (function entry addresses), for debugging.
    const std::vector<uint32_t>& call_stack() const { return call_stack_; }

    // Optional: record every memory write (address, size) for oracle diffs.
    void set_write_log(std::vector<std::pair<uint32_t, uint32_t>>* log) { write_log_ = log; }
    // Debugging: print the pc and call stack of every store to this word (0: off).
    void set_watch(uint32_t addr) { watch_ = addr & ~3u; }
    // Debugging: print the FPU registers whenever the pc reaches one of these addresses.
    void add_trace_pc(uint32_t pc) { trace_pcs_.push_back(pc); }
    // Debugging: print every pc in [lo, hi) as it executes (on: only while `on` is true: the caller picks the call).
    void set_pc_trace(uint32_t lo, uint32_t hi, bool on) { pctrace_lo_ = lo; pctrace_hi_ = hi; pctrace_on_ = on; }
    // Debugging: print `nwords` words at the address in GPR `reg` whenever the pc reaches `pc` (before it executes).
    void add_trace_mem(uint32_t pc, int reg, int nwords) { trace_mem_.push_back({pc, reg, nwords}); }
    // Debugging / oracle coverage: every executed pc (delay slots included) is inserted here (null: off).
    void set_cover(std::unordered_set<uint32_t>* c) { cover_ = c; }
    // Optional: record loads in [lo, hi) as (pc, address) pairs.
    void set_read_trace(uint32_t lo, uint32_t hi, std::vector<std::pair<uint32_t, uint32_t>>* log) {
        read_lo_ = lo; read_hi_ = hi; read_log_ = log;
    }

private:
    bool step();  // execute one instruction at cpu_.pc (handles delay slots)
    bool exec(uint32_t pc, uint32_t insn, uint32_t& next_pc, bool& branch_taken, bool& likely_skip);

    template <typename T> T load(uint32_t addr);
    uint32_t hw_read(uint32_t addr);
    void hw_write(uint32_t addr, uint32_t value);
    void dma_spr(int channel);
    template <typename T> void store(uint32_t addr, T v);
    void load128(uint32_t addr, u128& out);
    void store128(uint32_t addr, const u128& v);

    EeCpu cpu_;
    VUState vu0_{};
    VuMicroRun vu0_run_;
    bool vu0_coexec_ = false;
    int vu0_num_ = 1, vu0_den_ = 1, vu0_credit_ = 0;
    uint32_t cmsar0_ = 0;
    void vu0_tick();   // the running VU0 program's share of one EE instruction
    void vu0_drain();  // the EE waits for the running program's end
    std::unordered_map<uint32_t, Hook> hooks_;
    std::unordered_map<uint32_t, CondHook> cond_hooks_;
    std::unordered_map<uint32_t, Hook> probes_;
    uint32_t watch_ = 0;
    std::vector<uint32_t> trace_pcs_;
    uint32_t pctrace_lo_ = 0, pctrace_hi_ = 0;
    bool pctrace_on_ = false;
    struct TraceMem { uint32_t pc; int reg; int n; };
    std::vector<TraceMem> trace_mem_;
    std::unordered_set<uint32_t>* cover_ = nullptr;
    std::vector<uint8_t> stack_;
    std::vector<std::pair<uint32_t, uint32_t>>* write_log_ = nullptr;
    std::vector<std::pair<uint32_t, uint32_t>>* read_log_ = nullptr;
    uint32_t read_lo_ = 0, read_hi_ = 0;
    std::string error_;
    uint64_t executed_ = 0;
    std::unordered_map<uint32_t, uint32_t> hwregs_;  // 0x1000xxxx register file (DMA only is modelled)
    std::vector<uint32_t> call_stack_;
    uint32_t pc_ring_[64]{};
    uint32_t pc_ring_pos_ = 0;
};

// Translate an EE virtual address to a host pointer (RAM incl. uncached
// mirrors, or scratchpad). Returns nullptr for hardware registers.
void* ee_interp_ptr(uint32_t addr);

}  // namespace rac1

#endif
