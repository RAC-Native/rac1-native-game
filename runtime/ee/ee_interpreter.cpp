#include <cstdio>
// R5900 interpreter — see ee_interpreter.h.
// Instruction semantics follow the "EE Core Instruction Set Manual" (Sony,
// public) and PCSX2's interpreter behaviour where the manual is silent.

#include <runtime/ee/ee_interpreter.h>
#include <runtime/ee/ps2_fpu.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>

namespace rac1 {

namespace {

constexpr uint32_t RETURN_MAGIC = 0xFFFFFFF0u;  // $ra sentinel for EeInterpreter::call
constexpr uint32_t STACK_SIZE = 256 * 1024;
constexpr uint32_t STACK_TOP = 0x02000000u;     // 32 MB end: the game keeps level files below 0x1ff8000, the stack above

inline int64_t sx32(uint32_t v) { return static_cast<int64_t>(static_cast<int32_t>(v)); }
inline int64_t sx16(uint32_t v) { return static_cast<int64_t>(static_cast<int16_t>(v & 0xffff)); }

// PS2 FPU: no infinities/NaNs (results clamp to +-FLT_MAX), denormals flush to 0. Add / sub / mul
// round toward zero with the host's mode (ps2_fpu.h, set by the program), div / sqrt / rsqrt to nearest.
inline float ps2f(float x) {
    if (std::isnan(x) || std::isinf(x)) return std::signbit(x) ? -FLT_MAX : FLT_MAX;
    if (x != 0.0f && std::fabs(x) < FLT_MIN) return std::signbit(x) ? -0.0f : 0.0f;
    return x;
}
inline float u2f(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
inline uint32_t f2u(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
// A PS2 FPU operand: exponent 255 is an ordinary number (no Inf / NaN), denormals are zero.
inline double ps2in(float f) {
    const uint32_t u = f2u(f);
    const uint32_t e = u & 0x7f800000u;
    if (e == 0x7f800000u) {
        const double v = std::ldexp(1.0 + static_cast<double>(u & 0x7fffffu) / 8388608.0, 128);
        return (u >> 31) != 0 ? -v : v;
    }
    if (e == 0) return (u >> 31) != 0 ? -0.0 : 0.0;
    return static_cast<double>(f);
}
inline float ps2d(double r) {
    if (std::fabs(r) > static_cast<double>(FLT_MAX)) return std::signbit(r) ? -FLT_MAX : FLT_MAX;
    return ps2f(static_cast<float>(r));
}

struct Fmt { uint32_t i; };  // hex printer
std::ostream& operator<<(std::ostream& o, Fmt f) { char b[16]; std::snprintf(b, sizeof b, "0x%08x", f.i); return o << b; }

// VU0 micro memory as uploaded by the game's boot code. The three VIF MPG
// packets live in the boot ELF (SCUS_971.99, loaded at 0x100080); verified
// identical to a PCSX2 dump of VU0 micro memory taken during gameplay.
struct MpgPacket { uint32_t vif_code_addr; };
constexpr MpgPacket VU0_MPG_PACKETS[] = {{0x00100894}, {0x00100aec}, {0x0010e4cc}};

}  // namespace

void* ee_interp_ptr(uint32_t addr) {
    if ((addr >> 28) == 0x7) {
        if (addr - SPR_BASE < SPR_SIZE) return ee_get_scratchpad_base() + (addr - SPR_BASE);
        return nullptr;
    }
    const uint32_t phys = ee_mirror_phys(addr);  // kseg0/kseg1 and uncached mirrors: 32 MB RAM (a session's: its RAM)
    if (phys == 0xffffffffu) return nullptr;     // hardware registers
    return ee_get_ram_base() + phys;
}

static std::vector<uint64_t>& vu0_micromem() {
    static std::vector<uint64_t> mem;
    if (mem.empty()) {
        mem.assign(512, 0);  // VU0 micro memory: 4 KB
        for (const auto& p : VU0_MPG_PACKETS) {
            const uint32_t code = ee_read<uint32_t>(p.vif_code_addr);
            if (((code >> 24) & 0x7f) != 0x4a) continue;  // MPG
            uint32_t num = (code >> 16) & 0xff;
            if (num == 0) num = 256;
            const uint32_t at = code & 0xffff;
            for (uint32_t k = 0; k < num && at + k < mem.size(); ++k) {
                mem[at + k] = ee_read<uint64_t>(p.vif_code_addr + 4 + k * 8);
            }
        }
    }
    return mem;
}

EeInterpreter::EeInterpreter() : stack_(STACK_SIZE) { cpu_.vu0 = &vu0_; vu0_.vf[0].f[3] = 1.0f; }

template <typename T> T EeInterpreter::load(uint32_t addr) {
    if (read_log_ && addr >= read_lo_ && addr < read_hi_) read_log_->push_back({cpu_.pc, addr});
    void* p = ee_interp_ptr(addr);
    T v{};
    if (p) std::memcpy(&v, p, sizeof(T));
    else if ((addr & 0x1FFF0000u) == 0x10000000u) v = static_cast<T>(hw_read(addr & ~3u));
    return v;
}
template <typename T> void EeInterpreter::store(uint32_t addr, T v) {
    void* p = ee_interp_ptr(addr);
    if (!p) {
        if ((addr & 0x1FFF0000u) == 0x10000000u) hw_write(addr & 0x1FFFFFFCu, static_cast<uint32_t>(v));
        return;
    }
    std::memcpy(p, &v, sizeof(T));
    if (write_log_) write_log_->push_back({addr, sizeof(T)});
    if (watch_ != 0 && watch_ - (addr & ~3u) < sizeof(T) + (addr & 3u)) {  // debugging: who writes the watched word
        std::printf("  [watch %08x] pc %08x, stack:", watch_, cpu_.pc);
        for (uint32_t f : call_stack_) std::printf(" %08x", f);
        std::printf("\n");
    }
}

// --- Minimal DMA: only the scratchpad channels, executed synchronously -------
// The game kicks fromSPR (ch8, 0x1000d000) / toSPR (ch9, 0x1000d400) transfers
// and then spins on a sentinel word the transfer overwrites, so these must
// complete. Register layout: +0x00 CHCR, +0x10 MADR, +0x20 QWC, +0x30 TADR,
// +0x80 SADR (EE User's Manual, DMAC chapter).
uint32_t EeInterpreter::hw_read(uint32_t addr) {
    addr &= 0x1FFFFFFFu;
    auto it = hwregs_.find(addr);
    return it == hwregs_.end() ? 0u : it->second;
}

// VIF0 (DMA ch0, 0x10008000) in source chain mode: the VIF codes it carries are run here as
// far as the game uses them for VU0 micro programs (MPG; NOP / STCYCL / FLUSH* ignored).
void ee_vif0_chain(uint32_t tadr, bool tte) {
    auto& mm = vu0_micromem();
    std::vector<uint32_t> words;
    for (int guard = 0; guard < 4096; ++guard) {
        const uint64_t tag = ee_read<uint64_t>(tadr);
        const uint32_t qwc = static_cast<uint32_t>(tag & 0xffff), id = static_cast<uint32_t>(tag >> 28) & 7;
        const uint32_t ta = static_cast<uint32_t>(tag >> 32) & 0x7ffffff0u;
        if (tte) { words.push_back(ee_read<uint32_t>(tadr + 8)); words.push_back(ee_read<uint32_t>(tadr + 12)); }
        uint32_t from = 0;
        bool end = false;
        switch (id) {
        case 0: from = ta; end = true; break;
        case 1: from = tadr + 16; tadr += 16 + qwc * 16; break;
        case 2: from = tadr + 16; tadr = ta; break;
        case 3: case 4: from = ta; tadr += 16; break;
        case 7: from = tadr + 16; end = true; break;
        default: end = true; break;
        }
        for (uint32_t k = 0; k < qwc * 4; ++k) words.push_back(ee_read<uint32_t>(from + 4 * k));
        if (end) break;
    }
    for (size_t i = 0; i < words.size();) {
        const uint32_t code = words[i++];
        const uint32_t cmd = (code >> 24) & 0x7f;
        if (cmd == 0x4a) {  // MPG: num instructions (0 = 256) at the doubleword address
            uint32_t num = (code >> 16) & 0xff;
            if (num == 0) num = 256;
            const uint32_t at = code & 0xffff;
            for (uint32_t k = 0; k < num && i + 1 < words.size(); ++k, i += 2)
                if (at + k < mm.size()) mm[at + k] = words[i] | (static_cast<uint64_t>(words[i + 1]) << 32);
        } else if (cmd != 0 && cmd != 1 && cmd != 0x10 && cmd != 0x11 && cmd != 0x13) {
            std::cerr << "[ee_interpreter] VIF0 code " << std::hex << code << std::dec << " not handled\n";
            break;
        }
    }
}

void EeInterpreter::hw_write(uint32_t addr, uint32_t value) {
    addr &= 0x1FFFFFFFu;
    hwregs_[addr] = value;
    if (addr == 0x10008000u && (value & 0x100u)) {  // VIF0 DMA kicked
        if (((value >> 2) & 3) == 1) ee_vif0_chain(hwregs_[0x10008030u], (value & 0x40u) != 0);
        else std::cerr << "[ee_interpreter] VIF0 DMA mode " << ((value >> 2) & 3) << " not handled\n";
        hwregs_[addr] = value & ~0x100u;
        return;
    }
    if ((addr == 0x1000d000u || addr == 0x1000d400u) && (value & 0x100u)) {
        dma_spr(addr == 0x1000d000u ? 8 : 9);
        hwregs_[addr] = value & ~0x100u;  // STR cleared: transfer complete
    }
    // GIF (DMA ch2, 0x1000a000): the GS is not modelled, the transfer completes at once. The base ELF's
    // sceDmaSend-like 0x1221b0 spins on CHCR.STR before each kick; with STR left set, every send after the first one
    // of the process (0x200f40 without a packet: hero_lost's screen fade) spun 0x1000000 times, until the instruction
    // budget (or its timeout's printf), and the check stopped at that case.
    if (addr == 0x1000a000u && (value & 0x100u)) hwregs_[addr] = value & ~0x100u;
}

void EeInterpreter::dma_spr(int ch) {
    const uint32_t base = ch == 8 ? 0x1000d000u : 0x1000d400u;
    const uint32_t chcr = hwregs_[base];
    uint32_t madr = hwregs_[base + 0x10], qwc = hwregs_[base + 0x20] & 0xffff, tadr = hwregs_[base + 0x30];
    uint32_t sadr = hwregs_[base + 0x80] & 0x3ff0u;
    uint8_t* spr = ee_get_scratchpad_base();
    auto copy_q = [&](uint32_t ram, uint32_t n) {
        for (uint32_t q = 0; q < n; ++q) {
            uint8_t* r = static_cast<uint8_t*>(ee_interp_ptr((ram + q * 16) & ~15u));
            if (!r) break;
            uint8_t* s = spr + ((sadr + q * 16) & 0x3ff0u);
            if (ch == 9) std::memcpy(s, r, 16);
            else { std::memcpy(r, s, 16); if (write_log_) write_log_->push_back({(ram + q * 16) & ~15u, 16}); }
        }
        sadr = (sadr + n * 16) & 0x3ff0u;
    };
    const uint32_t mode = (chcr >> 2) & 3;
    if (mode == 0) {  // normal
        copy_q(madr, qwc);
        hwregs_[base + 0x20] = 0;
    } else if (mode == 1 && ch == 9) {  // source chain (toSPR)
        for (int guard = 0; guard < 65536; ++guard) {
            const uint64_t tag = load<uint64_t>(tadr);
            const uint32_t tq = static_cast<uint32_t>(tag & 0xffff), id = static_cast<uint32_t>(tag >> 28) & 7;
            const uint32_t ta = static_cast<uint32_t>(tag >> 32) & 0x7FFFFFF0u;
            bool end = false;
            switch (id) {
            case 0: copy_q(ta, tq); end = true; break;                         // refe
            case 1: copy_q(tadr + 16, tq); tadr += 16 + tq * 16; break;         // cnt
            case 2: copy_q(tadr + 16, tq); tadr = ta; break;                    // next
            case 3: case 4: copy_q(ta, tq); tadr += 16; break;                  // ref / refs
            case 7: copy_q(tadr + 16, tq); end = true; break;                   // end
            default: end = true; break;                                        // call/ret: not used here
            }
            if (end || ((tag >> 31) & 1 && (chcr & 0x80u))) break;              // IRQ + TIE stops the chain
        }
    } else {
        std::cerr << "[ee_interpreter] unsupported SPR DMA ch" << ch << " mode " << mode << "\n";
    }
    hwregs_[base + 0x80] = sadr;
}
void EeInterpreter::load128(uint32_t addr, u128& out) {
    addr &= ~15u;
    out.lo = load<uint64_t>(addr);
    out.hi = load<uint64_t>(addr + 8);
}
void EeInterpreter::store128(uint32_t addr, const u128& v) {
    addr &= ~15u;
    store<uint64_t>(addr, v.lo);
    store<uint64_t>(addr + 8, v.hi);
}

bool EeInterpreter::call(uint32_t addr, std::initializer_list<uint32_t> args, uint32_t gp, uint64_t max_instructions) {
    error_.clear();
    executed_ = 0;
    call_stack_.assign(1, addr);
    int r = 4;
    for (uint32_t a : args) { if (r <= 11) cpu_.set32(r++, static_cast<int32_t>(a)); }
    cpu_.set32(28, static_cast<int32_t>(gp));
    if (cpu_.r32(29) == 0) cpu_.set32(29, static_cast<int32_t>(STACK_TOP - 64));
    cpu_.set32(31, static_cast<int32_t>(RETURN_MAGIC));
    cpu_.pc = addr;
    while (cpu_.pc != RETURN_MAGIC) {
        if (executed_ >= max_instructions) {
            char b[160];
            std::snprintf(b, sizeof b, "instruction budget exceeded (pc %08x; VU0 %s pc %03x vi1 %d vi2 %d)", cpu_.pc, vu0_run_.running() ? "running" : "idle",
                          vu0_run_.pc(), vu0_.vi[1], vu0_.vi[2]);
            error_ = b;
            return false;
        }
        if (!step()) return false;
    }
    vu0_drain();  // (a program still running when the function returns: to its end)
    return true;
}

bool EeInterpreter::step() {
    if (!cond_hooks_.empty()) {
        const auto ch = cond_hooks_.find(cpu_.pc);
        if (ch != cond_hooks_.end() && ch->second(cpu_)) {
            cpu_.pc = cpu_.r32(31);
            if (!call_stack_.empty()) call_stack_.pop_back();
            return true;
        }
    }
    auto hit = hooks_.find(cpu_.pc);
    if (hit != hooks_.end()) {
        hit->second(cpu_);
        cpu_.pc = cpu_.r32(31);
        if (!call_stack_.empty()) call_stack_.pop_back();
        return true;
    }
    const uint32_t pc = cpu_.pc;
    pc_ring_[pc_ring_pos_++ & 63] = pc;
    if (cover_) cover_->insert(pc);
    for (uint32_t t : trace_pcs_) {
        if (t != pc) continue;
        std::printf("  [trace %08x]", pc);
        for (int r : {0, 1, 2, 3, 4, 12, 13, 14, 20, 21, 22, 23, 24, 25, 26}) std::printf(" f%d=%g (%08x)", r, cpu_.fpr[r], f2u(cpu_.fpr[r]));
        std::printf(" v0=%08x a0=%08x\n", cpu_.r32(2), cpu_.r32(4));
    }
    if (pctrace_on_ && pc >= pctrace_lo_ && pc < pctrace_hi_) std::printf("  [pc %08x]\n", pc);
    for (const TraceMem& t : trace_mem_) {
        if (t.pc != pc) continue;
        const uint32_t a = cpu_.r32(t.reg);
        std::printf("  [mem %08x] r%d=%08x:", pc, t.reg, a);
        for (int k = 0; k < t.n; ++k) {
            void* mp = ee_interp_ptr(a + 4u * static_cast<uint32_t>(k));
            uint32_t w = 0;
            if (mp) std::memcpy(&w, mp, 4);
            float f;
            std::memcpy(&f, &w, 4);
            std::printf(" %g(%08x)", f, w);
        }
        std::printf("\n");
    }
    void* ip = ee_interp_ptr(pc);
    if (!ip) { std::ostringstream s; s << "fetch from unmapped " << Fmt{pc}; error_ = s.str(); return false; }
    uint32_t insn; std::memcpy(&insn, ip, 4);
    uint32_t target = 0; bool taken = false, likely_skip = false;
    if (!exec(pc, insn, target, taken, likely_skip)) return false;
    ++executed_;
    vu0_tick();
    if (taken) {
        uint32_t ds; std::memcpy(&ds, ee_interp_ptr(pc + 4), 4);
        uint32_t t2 = 0; bool tk2 = false, ls2 = false;
        if (cover_) cover_->insert(pc + 4);
        if (!exec(pc + 4, ds, t2, tk2, ls2)) return false;
        ++executed_;
        vu0_tick();
        cpu_.pc = target;
    } else if (likely_skip) {
        cpu_.pc = pc + 8;
    } else {
        cpu_.pc = pc + 4;
    }
    return true;
}

void EeInterpreter::vu0_tick() {
    if (!vu0_coexec_ || !vu0_run_.running()) return;
    vu0_credit_ += vu0_num_;
    while (vu0_credit_ >= vu0_den_) {
        vu0_credit_ -= vu0_den_;
        if (!vu0_run_.step()) {
            vu0_run_.finish();
            vu0_credit_ = 0;
            return;
        }
    }
}

void EeInterpreter::vu0_drain() {
    if (!vu0_run_.running()) return;
    uint64_t n = 0;
    for (; n < 50'000'000 && vu0_run_.step(); ++n) {
    }
    if (n == 50'000'000) std::fprintf(stderr, "[ee_interpreter] VU0 drained by force at pc %03x (EE pc %08x): it waits on the EE\n", vu0_run_.pc(), cpu_.pc);
    vu0_run_.finish();
    vu0_credit_ = 0;
}

bool EeInterpreter::exec(uint32_t pc, uint32_t in, uint32_t& target, bool& taken, bool& likely_skip) {
    EeCpu& c = cpu_;
    const uint32_t op = in >> 26, rs = (in >> 21) & 31, rt = (in >> 16) & 31, rd = (in >> 11) & 31;
    const uint32_t sa = (in >> 6) & 31, fn = in & 63;
    const uint32_t imm = in & 0xffff;
    const int64_t simm = sx16(imm);
    auto R = [&](uint32_t r) -> uint64_t { return c.gpr[r].lo; };
    auto branch = [&](bool cond, bool likely) {
        if (cond) { taken = true; target = pc + 4 + static_cast<uint32_t>(simm << 2); }
        else if (likely) likely_skip = true;
    };
    auto unsupported = [&](const char* what) {
        std::ostringstream s; s << "unsupported " << what << " insn " << Fmt{in} << " at " << Fmt{pc};
        error_ = s.str();
        return false;
    };
    auto addr = [&]() { return static_cast<uint32_t>(R(rs) + static_cast<uint64_t>(simm)); };

    switch (op) {
    case 0x00: {  // SPECIAL
        switch (fn) {
        case 0x00: c.set32(rd, static_cast<int32_t>(c.r32(rt) << sa)); return true;                 // SLL
        case 0x02: c.set32(rd, static_cast<int32_t>(c.r32(rt) >> sa)); return true;                 // SRL
        case 0x03: c.set32(rd, c.s32(rt) >> sa); return true;                                       // SRA
        case 0x04: c.set32(rd, static_cast<int32_t>(c.r32(rt) << (R(rs) & 31))); return true;        // SLLV
        case 0x06: c.set32(rd, static_cast<int32_t>(c.r32(rt) >> (R(rs) & 31))); return true;        // SRLV
        case 0x07: c.set32(rd, c.s32(rt) >> (R(rs) & 31)); return true;                             // SRAV
        case 0x08: taken = true; target = c.r32(rs); if (rs == 31 && !call_stack_.empty()) call_stack_.pop_back(); return true;  // JR
        case 0x09: { uint32_t t = c.r32(rs); c.set32(rd ? rd : 31, static_cast<int32_t>(pc + 8)); taken = true; target = t; call_stack_.push_back(t); return true; }  // JALR
        case 0x0a: if (R(rt) == 0) c.set64(rd, R(rs)); return true;                                 // MOVZ
        case 0x0b: if (R(rt) != 0) c.set64(rd, R(rs)); return true;                                 // MOVN
        case 0x0c: return true;                                                                     // SYSCALL (ignored)
        case 0x0d: return true;                                                                     // BREAK (ignored)
        case 0x0f: return true;                                                                     // SYNC
        case 0x10: c.set64(rd, c.hi.lo); return true;                                               // MFHI
        case 0x11: c.hi.lo = R(rs); return true;                                                    // MTHI
        case 0x12: c.set64(rd, c.lo.lo); return true;                                               // MFLO
        case 0x13: c.lo.lo = R(rs); return true;                                                    // MTLO
        case 0x14: c.set64(rd, R(rt) << (R(rs) & 63)); return true;                                 // DSLLV
        case 0x16: c.set64(rd, R(rt) >> (R(rs) & 63)); return true;                                 // DSRLV
        case 0x17: c.set64(rd, static_cast<uint64_t>(static_cast<int64_t>(R(rt)) >> (R(rs) & 63))); return true;  // DSRAV
        case 0x18: { int64_t p = static_cast<int64_t>(c.s32(rs)) * c.s32(rt);                      // MULT (R5900: also rd)
                     c.lo.lo = sx32(static_cast<uint32_t>(p)); c.hi.lo = sx32(static_cast<uint32_t>(p >> 32));
                     c.set64(rd, c.lo.lo); return true; }
        case 0x19: { uint64_t p = static_cast<uint64_t>(c.r32(rs)) * c.r32(rt);                    // MULTU
                     c.lo.lo = sx32(static_cast<uint32_t>(p)); c.hi.lo = sx32(static_cast<uint32_t>(p >> 32));
                     c.set64(rd, c.lo.lo); return true; }
        case 0x1a: { int32_t a = c.s32(rs), b = c.s32(rt);                                          // DIV
                     if (b == 0) { c.lo.lo = sx32(a >= 0 ? 0xffffffffu : 1u); c.hi.lo = sx32(static_cast<uint32_t>(a)); }
                     else if (a == INT32_MIN && b == -1) { c.lo.lo = sx32(static_cast<uint32_t>(INT32_MIN)); c.hi.lo = 0; }
                     else { c.lo.lo = sx32(static_cast<uint32_t>(a / b)); c.hi.lo = sx32(static_cast<uint32_t>(a % b)); }
                     return true; }
        case 0x1b: { uint32_t a = c.r32(rs), b = c.r32(rt);                                         // DIVU
                     if (b == 0) { c.lo.lo = sx32(0xffffffffu); c.hi.lo = sx32(a); }
                     else { c.lo.lo = sx32(a / b); c.hi.lo = sx32(a % b); }
                     return true; }
        case 0x20: case 0x21: c.set32(rd, static_cast<int32_t>(c.r32(rs) + c.r32(rt))); return true;  // ADD/ADDU
        case 0x22: case 0x23: c.set32(rd, static_cast<int32_t>(c.r32(rs) - c.r32(rt))); return true;  // SUB/SUBU
        case 0x24: c.set64(rd, R(rs) & R(rt)); return true;
        case 0x25: c.set64(rd, R(rs) | R(rt)); return true;
        case 0x26: c.set64(rd, R(rs) ^ R(rt)); return true;
        case 0x27: c.set64(rd, ~(R(rs) | R(rt))); return true;
        case 0x28: c.set64(rd, c.sa); return true;                                                  // MFSA
        case 0x29: c.sa = R(rs); return true;                                                       // MTSA
        case 0x2a: c.set64(rd, static_cast<int64_t>(R(rs)) < static_cast<int64_t>(R(rt)) ? 1 : 0); return true;  // SLT
        case 0x2b: c.set64(rd, R(rs) < R(rt) ? 1 : 0); return true;                                 // SLTU
        case 0x2c: case 0x2d: c.set64(rd, R(rs) + R(rt)); return true;                              // DADD/DADDU
        case 0x2e: case 0x2f: c.set64(rd, R(rs) - R(rt)); return true;                              // DSUB/DSUBU
        case 0x34: return true;                                                                     // TEQ (no trap)
        case 0x38: c.set64(rd, R(rt) << sa); return true;                                           // DSLL
        case 0x3a: c.set64(rd, R(rt) >> sa); return true;                                           // DSRL
        case 0x3b: c.set64(rd, static_cast<uint64_t>(static_cast<int64_t>(R(rt)) >> sa)); return true;  // DSRA
        case 0x3c: c.set64(rd, R(rt) << (sa + 32)); return true;                                    // DSLL32
        case 0x3e: c.set64(rd, R(rt) >> (sa + 32)); return true;                                    // DSRL32
        case 0x3f: c.set64(rd, static_cast<uint64_t>(static_cast<int64_t>(R(rt)) >> (sa + 32))); return true;  // DSRA32
        default: return unsupported("SPECIAL");
        }
    }
    case 0x01: {  // REGIMM
        const int64_t v = static_cast<int64_t>(R(rs));
        switch (rt) {
        case 0x00: branch(v < 0, false); return true;   // BLTZ
        case 0x01: branch(v >= 0, false); return true;  // BGEZ
        case 0x02: branch(v < 0, true); return true;    // BLTZL
        case 0x03: branch(v >= 0, true); return true;   // BGEZL
        case 0x10: c.set32(31, static_cast<int32_t>(pc + 8)); branch(v < 0, false); return true;   // BLTZAL
        case 0x11: c.set32(31, static_cast<int32_t>(pc + 8)); branch(v >= 0, false); return true;  // BGEZAL
        case 0x18: c.sa = (R(rs) + imm) & 15; return true;                                         // MTSAB
        case 0x19: c.sa = ((R(rs) + imm) & 7) * 2; return true;                                    // MTSAH
        default: return unsupported("REGIMM");
        }
    }
    case 0x02: taken = true; target = (pc & 0xf0000000u) | ((in & 0x03ffffffu) << 2); return true;   // J
    case 0x03: c.set32(31, static_cast<int32_t>(pc + 8)); taken = true; target = (pc & 0xf0000000u) | ((in & 0x03ffffffu) << 2); call_stack_.push_back(target); return true;  // JAL
    case 0x04: branch(R(rs) == R(rt), false); return true;                                   // BEQ
    case 0x05: branch(R(rs) != R(rt), false); return true;                                   // BNE
    case 0x06: branch(static_cast<int64_t>(R(rs)) <= 0, false); return true;                  // BLEZ
    case 0x07: branch(static_cast<int64_t>(R(rs)) > 0, false); return true;                   // BGTZ
    case 0x08: case 0x09: c.set32(rt, static_cast<int32_t>(c.r32(rs) + static_cast<uint32_t>(simm))); return true;  // ADDI/ADDIU
    case 0x0a: c.set64(rt, static_cast<int64_t>(R(rs)) < simm ? 1 : 0); return true;          // SLTI
    case 0x0b: c.set64(rt, R(rs) < static_cast<uint64_t>(simm) ? 1 : 0); return true;         // SLTIU
    case 0x0c: c.set64(rt, R(rs) & imm); return true;                                        // ANDI
    case 0x0d: c.set64(rt, R(rs) | imm); return true;                                        // ORI
    case 0x0e: c.set64(rt, R(rs) ^ imm); return true;                                        // XORI
    case 0x0f: c.set32(rt, static_cast<int32_t>(imm << 16)); return true;                    // LUI
    case 0x10: {  // COP0: only branch-on-COP0 and EI/DI/ERET-like no-ops appear
        if (rs == 0x08) { branch((rt & 1) != 0, (rt & 2) != 0); return true; }  // BC0F/BC0T: assume CPCOND0 = 1
        if (rs == 0x00) { c.set64(rt, 0); return true; }                       // MFC0 -> 0
        return true;                                                            // MTC0 / TLB ops / EI / DI ignored
    }
    case 0x11: {  // COP1
        const uint32_t ft = rt, fs = rd, fd = sa;
        switch (rs) {
        case 0x00: c.set32(rt, static_cast<int32_t>(f2u(c.fpr[fs]))); return true;            // MFC1
        case 0x02: c.set32(rt, fs == 31 ? (c.fcr_c ? 0x00800000 : 0) : 0); return true;       // CFC1
        case 0x04: c.fpr[fs] = u2f(c.r32(rt)); return true;                                   // MTC1
        case 0x06: if (fs == 31) c.fcr_c = (c.r32(rt) >> 23) & 1; return true;               // CTC1
        case 0x08: branch(((rt & 1) != 0) == c.fcr_c, (rt & 2) != 0); return true;            // BC1F/T(L)
        case 0x10: {
            float& d = c.fpr[fd]; const float s = c.fpr[fs], t = c.fpr[ft];
            switch (fn) {
            case 0x00: d = ps2d(ps2in(s) + ps2in(t)); return true;
            case 0x01: d = ps2d(ps2in(s) - ps2in(t)); return true;
            case 0x02: d = ps2d(ps2in(s) * ps2in(t)); return true;
            case 0x03: d = (ps2in(t) == 0.0) ? ((std::signbit(s) != std::signbit(t)) ? -FLT_MAX : FLT_MAX) : ps2d(ps2fpu::div_nearest(ps2in(s), ps2in(t))); return true;  // DIV.S (to nearest)
            case 0x04: d = ps2f(ps2fpu::sqrt_nearest(std::fabs(t))); return true;              // SQRT (fd <- sqrt(ft), to nearest)
            case 0x05: d = std::fabs(s); return true;
            case 0x06: d = s; return true;
            case 0x07: d = -s; return true;
            case 0x16: { float q = ps2fpu::sqrt_nearest(std::fabs(t)); d = q == 0.0f ? (std::signbit(s) ? -FLT_MAX : FLT_MAX) : ps2f(ps2fpu::div_nearest(s, q)); return true; }  // RSQRT (to nearest)
            case 0x18: c.acc = ps2f(s + t); return true;                                      // ADDA
            case 0x19: c.acc = ps2f(s - t); return true;                                      // SUBA
            case 0x1a: c.acc = ps2f(s * t); return true;                                      // MULA
            case 0x1c: d = ps2f(c.acc + ps2f(s * t)); return true;                            // MADD
            case 0x1d: d = ps2f(c.acc - ps2f(s * t)); return true;                            // MSUB
            case 0x1e: c.acc = ps2f(c.acc + ps2f(s * t)); return true;                        // MADDA
            case 0x1f: c.acc = ps2f(c.acc - ps2f(s * t)); return true;                        // MSUBA
            case 0x24: {                                                                      // CVT.W.S (truncate, saturate)
                int32_t w;
                if (s >= 2147483648.0f) w = INT32_MAX; else if (s <= -2147483648.0f) w = INT32_MIN; else w = static_cast<int32_t>(s);
                d = u2f(static_cast<uint32_t>(w)); return true; }
            case 0x28: d = std::max(s, t); return true;                                       // MAX
            case 0x29: d = std::min(s, t); return true;                                       // MIN
            case 0x30: c.fcr_c = false; return true;                                          // C.F
            case 0x32: c.fcr_c = (s == t); return true;                                       // C.EQ
            case 0x34: c.fcr_c = (s < t); return true;                                        // C.LT
            case 0x36: c.fcr_c = (s <= t); return true;                                       // C.LE
            default: return unsupported("COP1.S");
            }
        }
        case 0x14:
            if (fn == 0x20) { c.fpr[fd] = static_cast<float>(static_cast<int32_t>(f2u(c.fpr[fs]))); return true; }  // CVT.S.W
            return unsupported("COP1.W");
        default: return unsupported("COP1");
        }
    }
    case 0x12: {  // COP2 (VU0 macro mode)
        if (rs == 0x08) return unsupported("BC2");
        if (rs >= 0x10 && (fn == 0x38 || fn == 0x39)) {                                       // VCALLMS / VCALLMSR
            uint32_t start = (fn == 0x38) ? ((in >> 6) & 0x7fff) : (vu0_coexec_ ? cmsar0_ : static_cast<uint32_t>(static_cast<uint16_t>(vu0_.vi[11])));  // (without co-execution CMSAR0 read as vi[11], as before)
            auto& mm = vu0_micromem();
            if (vu0_coexec_) {  // the EE waits for a running program, then starts this one beside it
                vu0_drain();
                vu0_run_.start(vu0_, mm.data(), mm.size(), start);
                return true;
            }
            execute_microprogram(vu0_, mm.data(), mm.size(), start);
            return true;
        }
        if (vu0_coexec_ && vu0_run_.running()) {
            // macro ops and interlocked transfers (.I) wait for the program; plain QMFC2 / CFC2 /
            // QMTC2 / CTC2 reach its registers as they are
            const bool transfer = rs == 0x01 || rs == 0x02 || rs == 0x05 || rs == 0x06;
            if (!transfer || (in & 1) != 0) vu0_drain();
        }
        if (vu0_coexec_ && rd == 27 && (rs == 0x02 || rs == 0x06)) {  // CMSAR0
            if (rs == 0x06) cmsar0_ = c.r32(rt) & 0xffff;
            else c.set32(rt, static_cast<int32_t>(cmsar0_));
            return true;
        }
        execute_cop2_instruction(vu0_, in, c.gpr, true);
        return true;
    }
    case 0x14: branch(R(rs) == R(rt), true); return true;                                    // BEQL
    case 0x15: branch(R(rs) != R(rt), true); return true;                                    // BNEL
    case 0x16: branch(static_cast<int64_t>(R(rs)) <= 0, true); return true;                   // BLEZL
    case 0x17: branch(static_cast<int64_t>(R(rs)) > 0, true); return true;                    // BGTZL
    case 0x18: case 0x19: c.set64(rt, R(rs) + static_cast<uint64_t>(simm)); return true;      // DADDI/DADDIU
    case 0x1c: {  // MMI
        u128& d = c.gpr[rd]; const u128 s = c.gpr[rs], t = c.gpr[rt];
        auto w = [](const u128& v, int i) -> uint32_t { return static_cast<uint32_t>((i < 2 ? v.lo : v.hi) >> ((i & 1) * 32)); };
        auto h = [](const u128& v, int i) -> uint16_t { return static_cast<uint16_t>((i < 4 ? v.lo : v.hi) >> ((i & 3) * 16)); };
        auto b = [](const u128& v, int i) -> uint8_t { return static_cast<uint8_t>((i < 8 ? v.lo : v.hi) >> ((i & 7) * 8)); };
        auto mkw = [](uint32_t a, uint32_t b_, uint32_t c_, uint32_t d_) { return u128{(static_cast<uint64_t>(b_) << 32) | a, (static_cast<uint64_t>(d_) << 32) | c_}; };
        auto set = [&](const u128& v) { if (rd != 0) d = v; };
        switch (fn) {
        case 0x08: {  // MMI0
            switch (sa) {
            case 0x00: set(mkw(w(s,0)+w(t,0), w(s,1)+w(t,1), w(s,2)+w(t,2), w(s,3)+w(t,3))); return true;  // PADDW
            case 0x01: set(mkw(w(s,0)-w(t,0), w(s,1)-w(t,1), w(s,2)-w(t,2), w(s,3)-w(t,3))); return true;  // PSUBW
            case 0x03: { uint32_t r[4]; for (int i = 0; i < 4; ++i) r[i] = std::max<int32_t>(static_cast<int32_t>(w(s,i)), static_cast<int32_t>(w(t,i))); set(mkw(r[0],r[1],r[2],r[3])); return true; }  // PMAXW
            case 0x04: { u128 r{}; for (int i = 0; i < 8; ++i) { uint64_t v = static_cast<uint16_t>(h(s,i) + h(t,i)); (i < 4 ? r.lo : r.hi) |= v << ((i & 3) * 16); } set(r); return true; }  // PADDH
            case 0x05: { u128 r{}; for (int i = 0; i < 8; ++i) { uint64_t v = static_cast<uint16_t>(h(s,i) - h(t,i)); (i < 4 ? r.lo : r.hi) |= v << ((i & 3) * 16); } set(r); return true; }  // PSUBH
            case 0x08: case 0x09: { u128 r{}; for (int i = 0; i < 16; ++i) { const uint8_t v = static_cast<uint8_t>(sa == 0x08 ? b(s,i) + b(t,i) : b(s,i) - b(t,i)); (i < 8 ? r.lo : r.hi) |= static_cast<uint64_t>(v) << ((i & 7) * 8); } set(r); return true; }  // PADDB / PSUBB
            case 0x12: set(mkw(w(t,0), w(s,0), w(t,1), w(s,1))); return true;                   // PEXTLW
            case 0x13: set(mkw(w(t,0), w(t,2), w(s,0), w(s,2))); return true;                   // PPACW
            case 0x16: { u128 r{}; for (int i = 0; i < 4; ++i) { uint64_t lo = h(t,i), hi = h(s,i); (i < 2 ? r.lo : r.hi) |= (lo | (hi << 16)) << ((i & 1) * 32); } set(r); return true; }  // PEXTLH
            case 0x17: { u128 r{}; for (int i = 0; i < 4; ++i) { r.lo |= static_cast<uint64_t>(h(t, i * 2)) << (i * 16); r.hi |= static_cast<uint64_t>(h(s, i * 2)) << (i * 16); } set(r); return true; }  // PPACH
            case 0x19: { u128 r{}; for (int i = 0; i < 16; ++i) { int v = static_cast<int8_t>(b(s,i)) - static_cast<int8_t>(b(t,i)); v = std::clamp(v, -128, 127); (i < 8 ? r.lo : r.hi) |= static_cast<uint64_t>(static_cast<uint8_t>(v)) << ((i & 7) * 8); } set(r); return true; }  // PSUBSB
            case 0x1a: { u128 r{}; for (int i = 0; i < 8; ++i) { uint64_t lo = b(t,i), hi = b(s,i); (i < 4 ? r.lo : r.hi) |= (lo | (hi << 8)) << ((i & 3) * 16); } set(r); return true; }  // PEXTLB
            case 0x1b: { u128 r{}; for (int i = 0; i < 8; ++i) { r.lo |= static_cast<uint64_t>(b(t, i * 2)) << (i * 8); r.hi |= static_cast<uint64_t>(b(s, i * 2)) << (i * 8); } set(r); return true; }  // PPACB
            case 0x1e: { uint32_t r[4]; for (int i = 0; i < 4; ++i) { const uint32_t x = w(t,i); r[i] = ((x & 0x1fu) << 3) | ((x & 0x3e0u) << 6) | ((x & 0x7c00u) << 9) | ((x & 0x8000u) << 16); } set(mkw(r[0],r[1],r[2],r[3])); return true; }  // PEXT5
            default: return unsupported("MMI0");
            }
        }
        case 0x09: {  // MMI2
            switch (sa) {
            case 0x0e: set(u128{t.lo, s.lo}); return true;                                      // PCPYLD
            case 0x12: set(u128{s.lo & t.lo, s.hi & t.hi}); return true;                        // PAND
            case 0x13: set(u128{s.lo ^ t.lo, s.hi ^ t.hi}); return true;                        // PXOR
            case 0x1f: set(mkw(w(t,1), w(t,2), w(t,0), w(t,3))); return true;                   // PROT3W
            case 0x1e: set(mkw(w(t,2), w(t,1), w(t,0), w(t,3))); return true;                   // PEXEW
            case 0x1c: {                                                                         // PMULTH
                uint32_t p[8];
                for (int i = 0; i < 8; ++i) p[i] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(h(s,i))) * static_cast<int32_t>(static_cast<int16_t>(h(t,i))));
                c.lo = mkw(p[0], p[1], p[4], p[5]);
                c.hi = mkw(p[2], p[3], p[6], p[7]);
                set(mkw(p[0], p[2], p[4], p[6]));
                return true;
            }
            default: return unsupported("MMI2");
            }
        }
        case 0x28: {  // MMI1
            switch (sa) {
            case 0x02: { uint32_t r[4]; for (int i = 0; i < 4; ++i) r[i] = w(s,i) == w(t,i) ? 0xffffffffu : 0; set(mkw(r[0],r[1],r[2],r[3])); return true; }  // PCEQW
            case 0x03: { uint32_t r[4]; for (int i = 0; i < 4; ++i) r[i] = std::min<int32_t>(static_cast<int32_t>(w(s,i)), static_cast<int32_t>(w(t,i))); set(mkw(r[0],r[1],r[2],r[3])); return true; }  // PMINW
            case 0x12: set(mkw(w(t,2), w(s,2), w(t,3), w(s,3))); return true;                   // PEXTUW
            case 0x16: { u128 r{}; for (int i = 0; i < 4; ++i) { uint64_t lo = h(t,i+4), hi = h(s,i+4); (i < 2 ? r.lo : r.hi) |= (lo | (hi << 16)) << ((i & 1) * 32); } set(r); return true; }  // PEXTUH
            case 0x18: { u128 r{}; for (int i = 0; i < 16; ++i) { uint32_t v = b(s,i) + b(t,i); if (v > 0xff) v = 0xff; (i < 8 ? r.lo : r.hi) |= static_cast<uint64_t>(v) << ((i & 7) * 8); } set(r); return true; }  // PADDUB
            case 0x1a: { u128 r{}; for (int i = 0; i < 8; ++i) { uint64_t lo = b(t,i+8), hi = b(s,i+8); (i < 4 ? r.lo : r.hi) |= (lo | (hi << 8)) << ((i & 3) * 16); } set(r); return true; }  // PEXTUB
            case 0x10: { uint32_t r[4]; for (int i = 0; i < 4; ++i) { const uint64_t v = static_cast<uint64_t>(w(s,i)) + w(t,i); r[i] = v > 0xffffffffull ? 0xffffffffu : static_cast<uint32_t>(v); } set(mkw(r[0],r[1],r[2],r[3])); return true; }  // PADDUW
            default: return unsupported("MMI1");
            }
        }
        case 0x29: {  // MMI3
            switch (sa) {
            case 0x0e: set(u128{s.hi, t.hi}); return true;                                      // PCPYUD
            case 0x0c: {                                                                         // PMULTUW
                const uint64_t p0 = static_cast<uint64_t>(w(s,0)) * w(t,0), p1 = static_cast<uint64_t>(w(s,2)) * w(t,2);
                auto sx = [](uint64_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); };
                c.lo = u128{sx(p0), sx(p1)};
                c.hi = u128{sx(p0 >> 32), sx(p1 >> 32)};
                set(u128{p0, p1});
                return true;
            }
            case 0x12: set(u128{s.lo | t.lo, s.hi | t.hi}); return true;                        // POR
            case 0x1a: { u128 r{}; const int o[8] = {0, 2, 1, 3, 4, 6, 5, 7}; for (int i = 0; i < 8; ++i) (i < 4 ? r.lo : r.hi) |= static_cast<uint64_t>(h(t, o[i])) << ((i & 3) * 16); set(r); return true; }  // PEXCH
            case 0x13: set(u128{~(s.lo | t.lo), ~(s.hi | t.hi)}); return true;                 // PNOR
            case 0x1b: { uint64_t l = t.lo & 0xffff, hh = t.hi & 0xffff; l |= l << 16; l |= l << 32; hh |= hh << 16; hh |= hh << 32; set(u128{l, hh}); return true; }  // PCPYH
            default: return unsupported("MMI3");
            }
        }
        case 0x36: { u128 r{}; for (int i = 0; i < 8; ++i) { uint16_t v = static_cast<uint16_t>(h(t,i) >> sa); (i < 4 ? r.lo : r.hi) |= static_cast<uint64_t>(v) << ((i & 3) * 16); } set(r); return true; }  // PSRLH
        case 0x37: { u128 r{}; for (int i = 0; i < 8; ++i) { int16_t v = static_cast<int16_t>(h(t,i)) >> sa; (i < 4 ? r.lo : r.hi) |= static_cast<uint64_t>(static_cast<uint16_t>(v)) << ((i & 3) * 16); } set(r); return true; }  // PSRAH
        case 0x3c: set(mkw(w(t,0) << sa, w(t,1) << sa, w(t,2) << sa, w(t,3) << sa)); return true;   // PSLLW
        case 0x3e: set(mkw(w(t,0) >> sa, w(t,1) >> sa, w(t,2) >> sa, w(t,3) >> sa)); return true;   // PSRLW
        case 0x3f: { uint32_t r[4]; for (int i = 0; i < 4; ++i) r[i] = static_cast<uint32_t>(static_cast<int32_t>(w(t,i)) >> sa); set(mkw(r[0],r[1],r[2],r[3])); return true; }  // PSRAW
        case 0x00: case 0x01: case 0x20: case 0x21: {                                            // MADD / MADDU / MADD1 / MADDU1
            const bool pipe1 = fn >= 0x20, uns = (fn & 1) != 0;
            uint64_t& L = pipe1 ? c.lo.hi : c.lo.lo; uint64_t& H = pipe1 ? c.hi.hi : c.hi.lo;
            const int64_t acc = static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(H)) << 32) | static_cast<uint32_t>(L));
            const int64_t p = uns ? static_cast<int64_t>(static_cast<uint64_t>(c.r32(rs)) * c.r32(rt))
                                  : static_cast<int64_t>(c.s32(rs)) * c.s32(rt);
            const int64_t r = acc + p;
            L = sx32(static_cast<uint32_t>(r)); H = sx32(static_cast<uint32_t>(static_cast<uint64_t>(r) >> 32));
            c.set64(rd, L); return true; }
        case 0x04: { auto cnt = [](uint32_t v) { uint32_t n = 0; const uint32_t sgn = v >> 31; for (int i = 30; i >= 0 && ((v >> i) & 1) == sgn; --i) ++n; return n; };  // PLZCW
                     c.set64(rd, (static_cast<uint64_t>(cnt(static_cast<uint32_t>(R(rs) >> 32))) << 32) | cnt(c.r32(rs))); return true; }
        case 0x30: {                                                                             // PMFHL
            if (sa == 0) { set(mkw(w(c.lo,0), w(c.hi,0), w(c.lo,2), w(c.hi,2))); return true; }  // .LW
            if (sa == 1) { set(mkw(w(c.lo,1), w(c.hi,1), w(c.lo,3), w(c.hi,3))); return true; }  // .UW
            if (sa == 4) {  // .SH: LO / HI words saturated to halfwords, in order
                auto s16 = [](uint32_t v) { const int32_t x = static_cast<int32_t>(v); return static_cast<uint16_t>(x > 32767 ? 32767 : x < -32768 ? -32768 : x); };
                const uint16_t r[8] = {s16(w(c.lo,0)), s16(w(c.lo,1)), s16(w(c.hi,0)), s16(w(c.hi,1)), s16(w(c.lo,2)), s16(w(c.lo,3)), s16(w(c.hi,2)), s16(w(c.hi,3))};
                u128 o{};
                for (int i = 0; i < 8; ++i) (i < 4 ? o.lo : o.hi) |= static_cast<uint64_t>(r[i]) << ((i & 3) * 16);
                set(o);
                return true;
            }
            return unsupported("PMFHL");
        }
        case 0x10: c.set64(rd, c.hi.hi); return true;                                           // MFHI1
        case 0x12: c.set64(rd, c.lo.hi); return true;                                           // MFLO1
        case 0x18: { int64_t p = static_cast<int64_t>(c.s32(rs)) * c.s32(rt);                    // MULT1
                     c.lo.hi = sx32(static_cast<uint32_t>(p)); c.hi.hi = sx32(static_cast<uint32_t>(p >> 32)); c.set64(rd, c.lo.hi); return true; }
        case 0x1a: { int32_t a = c.s32(rs), bb = c.s32(rt);                                      // DIV1
                     if (bb != 0 && !(a == INT32_MIN && bb == -1)) { c.lo.hi = sx32(static_cast<uint32_t>(a / bb)); c.hi.hi = sx32(static_cast<uint32_t>(a % bb)); }
                     return true; }
        default: return unsupported("MMI");
        }
    }
    case 0x1a: case 0x1b: {  // LDL / LDR
        const uint32_t a = addr(); const uint32_t sh = (a & 7) * 8;
        const uint64_t mem = load<uint64_t>(a & ~7u);
        uint64_t v = R(rt);
        if (op == 0x1a) { v = (sh == 56) ? mem : ((v & ((1ull << (56 - sh)) - 1)) | (mem << (56 - sh))); }
        else { v = (sh == 0) ? mem : ((v & ~(~0ull >> sh)) | (mem >> sh)); }
        c.set64(rt, v); return true;
    }
    case 0x1e: { u128 v; load128(addr(), v); if (rt) c.gpr[rt] = v; return true; }           // LQ
    case 0x1f: store128(addr(), c.gpr[rt]); return true;                                     // SQ
    case 0x20: c.set64(rt, static_cast<uint64_t>(static_cast<int64_t>(load<int8_t>(addr())))); return true;   // LB
    case 0x21: c.set64(rt, static_cast<uint64_t>(static_cast<int64_t>(load<int16_t>(addr())))); return true;  // LH
    case 0x22: case 0x26: {  // LWL / LWR
        const uint32_t a = addr(); const uint32_t sh = (a & 3) * 8;
        const uint32_t mem = load<uint32_t>(a & ~3u);
        uint32_t v = c.r32(rt);
        if (op == 0x22) v = (sh == 24) ? mem : ((v & ((1u << (24 - sh)) - 1)) | (mem << (24 - sh)));
        else v = (sh == 0) ? mem : ((v & ~(~0u >> sh)) | (mem >> sh));
        c.set32(rt, static_cast<int32_t>(v)); return true;
    }
    case 0x23: c.set32(rt, load<int32_t>(addr())); return true;                              // LW
    case 0x24: c.set64(rt, load<uint8_t>(addr())); return true;                              // LBU
    case 0x25: c.set64(rt, load<uint16_t>(addr())); return true;                             // LHU
    case 0x27: c.set64(rt, load<uint32_t>(addr())); return true;                             // LWU
    case 0x28: store<uint8_t>(addr(), static_cast<uint8_t>(R(rt))); return true;             // SB
    case 0x29: store<uint16_t>(addr(), static_cast<uint16_t>(R(rt))); return true;           // SH
    case 0x2a: case 0x2e: {  // SWL / SWR
        const uint32_t a = addr(); const uint32_t sh = (a & 3) * 8;
        uint32_t mem = load<uint32_t>(a & ~3u); const uint32_t v = c.r32(rt);
        if (op == 0x2a) mem = (sh == 24) ? v : ((mem & ~(~0u >> (24 - sh))) | (v >> (24 - sh)));
        else mem = (sh == 0) ? v : ((mem & ((1u << sh) - 1)) | (v << sh));
        store<uint32_t>(a & ~3u, mem); return true;
    }
    case 0x2b: store<uint32_t>(addr(), c.r32(rt)); return true;                              // SW
    case 0x2c: case 0x2d: {  // SDL / SDR
        const uint32_t a = addr(); const uint32_t sh = (a & 7) * 8;
        uint64_t mem = load<uint64_t>(a & ~7u); const uint64_t v = R(rt);
        if (op == 0x2c) mem = (sh == 56) ? v : ((mem & ~(~0ull >> (56 - sh))) | (v >> (56 - sh)));
        else mem = (sh == 0) ? v : ((mem & ((1ull << sh) - 1)) | (v << sh));
        store<uint64_t>(a & ~7u, mem); return true;
    }
    case 0x2f: return true;                                                                  // CACHE
    case 0x31: c.fpr[rt] = u2f(load<uint32_t>(addr())); return true;                         // LWC1
    case 0x33: return true;                                                                  // PREF
    case 0x36: { u128 v; load128(addr(), v); if (rt) { vu0_.vf[rt].u[0] = static_cast<uint32_t>(v.lo); vu0_.vf[rt].u[1] = static_cast<uint32_t>(v.lo >> 32); vu0_.vf[rt].u[2] = static_cast<uint32_t>(v.hi); vu0_.vf[rt].u[3] = static_cast<uint32_t>(v.hi >> 32); } return true; }  // LQC2
    case 0x37: c.set64(rt, load<uint64_t>(addr())); return true;                             // LD
    case 0x39: store<uint32_t>(addr(), f2u(c.fpr[rt])); return true;                         // SWC1
    case 0x3e: { const auto& v = vu0_.vf[rt]; store128(addr(), u128{(static_cast<uint64_t>(v.u[1]) << 32) | v.u[0], (static_cast<uint64_t>(v.u[3]) << 32) | v.u[2]}); return true; }  // SQC2
    case 0x3f: store<uint64_t>(addr(), R(rt)); return true;                                  // SD
    default: return unsupported("opcode");
    }
}

std::vector<uint32_t> EeInterpreter::recent_pcs() const {
    std::vector<uint32_t> v;
    for (uint32_t k = 0; k < 64; ++k) v.push_back(pc_ring_[(pc_ring_pos_ + k) & 63]);
    return v;
}

}  // namespace rac1
