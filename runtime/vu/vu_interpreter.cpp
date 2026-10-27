#include <runtime/vu/vu_interpreter.h>
#include "vu_xgkick.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace rac1 {

// -------------------------------------------------------------------------
//  PS2 Non-IEEE Floating Point Clamping
// -------------------------------------------------------------------------

static float ps2_clamp_operand(uint32_t u) {
    uint32_t exp = u & 0x7f800000;
    if (exp == 0) {
        // Flush denormals to signed zero
        u &= 0x80000000;
    } else if (exp == 0x7f800000) {
        // Clamp Inf/NaN to signed max float representation
        u = (u & 0x80000000) | 0x7f7fffff;
    }
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

static float ps2_clamp_float(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return ps2_clamp_operand(u);
}

// -------------------------------------------------------------------------
//  MAC and Status Register Flags Handling
// -------------------------------------------------------------------------

static uint32_t update_mac_flag(VUState& state, int component, float f) {
    uint32_t v;
    std::memcpy(&v, &f, sizeof(v));
    int shift = 3 - component; // component 0->x (shift 3), 1->y (shift 2), 2->z (shift 1), 3->w (shift 0)
    uint32_t s = v & 0x80000000;

    // Update Sign flag (Sx, Sy, Sz, Sw -> bits 7, 6, 5, 4 of macflag)
    if (s) {
        state.macflag |= (0x0010 << shift);
    } else {
        state.macflag &= ~(0x0010 << shift);
    }

    if (f == 0.0f) {
        // Update Zero flag (Zx, Zy, Zz, Zw -> bits 3, 2, 1, 0)
        // Clear Overflow (Ox, Oy, Oz, Ow -> bits 15, 14, 13, 12) and Underflow (Ux, Uy, Uz, Uw -> bits 11, 10, 9, 8)
        state.macflag = (state.macflag & ~(0x1100 << shift)) | (0x0001 << shift);
        return v;
    }

    int exp = (v >> 23) & 0xff;
    switch (exp) {
        case 0:
            // Underflow + Zero
            state.macflag = (state.macflag & ~(0x1000 << shift)) | (0x0101 << shift);
            return s; // Returns signed zero
        case 255:
            // Overflow
            state.macflag = (state.macflag & ~(0x0101 << shift)) | (0x1000 << shift);
            return s | 0x7f7fffff; // Returns signed max float
        default:
            // Normal float, clear Z, U, O flags
            state.macflag &= ~(0x1101 << shift);
            return v;
    }
}

static void clear_mac_flag(VUState& state, int component) {
    int shift = 3 - component;
    state.macflag &= ~(0x1111 << shift);
}

static void update_status_flag(VUState& state) {
    uint32_t new_mac_status = 0;
    if (state.macflag & 0x000F) new_mac_status |= 0x1; // Z (zero flag)
    if (state.macflag & 0x00F0) new_mac_status |= 0x2; // S (sign flag)
    if (state.macflag & 0x0F00) new_mac_status |= 0x4; // U (underflow flag)
    if (state.macflag & 0xF000) new_mac_status |= 0x8; // O (overflow flag)
    // Update current flags (bits 3-0) and accumulate sticky flags (bits 9-6)
    state.statusflag = (state.statusflag & 0xFC0) | new_mac_status | (new_mac_status << 6);
}

static void update_status_flag_efu(VUState& state, uint32_t efu_flags) {
    // efu_flags contains bits 4 (I) and 5 (D)
    // Synchronize current EFU flags (bits 5-4) and sticky EFU flags (bits 11-10)
    state.statusflag = (state.statusflag & 0x3CF) | efu_flags | (efu_flags << 6);
}

// -------------------------------------------------------------------------
//  Min/Max Semantics
// -------------------------------------------------------------------------

static uint32_t fp_max(uint32_t a, uint32_t b) {
    return ((int32_t)a < 0 && (int32_t)b < 0) ? std::min<int32_t>(a, b) : std::max<int32_t>(a, b);
}

static uint32_t fp_min(uint32_t a, uint32_t b) {
    return ((int32_t)a < 0 && (int32_t)b < 0) ? std::max<int32_t>(a, b) : std::min<int32_t>(a, b);
}

// -------------------------------------------------------------------------
//  LFSR Random Generator
// -------------------------------------------------------------------------

static void advance_lfsr(VUState& state) {
    uint32_t r_u;
    std::memcpy(&r_u, &state.r, sizeof(r_u));
    int x = (r_u >> 4) & 1;
    int y = (r_u >> 22) & 1;
    uint32_t lfsr = ((r_u << 1) ^ (x ^ y)) & 0x7fffff;
    r_u = 0x3f800000 | lfsr;
    std::memcpy(&state.r, &r_u, sizeof(state.r));
}

// -------------------------------------------------------------------------
//  Main COP2 Instruction execution
// -------------------------------------------------------------------------

void execute_cop2_instruction(VUState& state, uint32_t opcode, u128* ee_gprs, bool is_macro_mode) {
    bool is_cop2_move = false;
    uint32_t op = (opcode >> 26) & 0x3f;
    if (is_macro_mode || op == 0x12) {
        if (op != 0x12) {
            return; // Not a COP2 instruction
        }
        // In macro/COP2 mode, bit 25 differentiates between COP2 move (0) and VU instruction (1)
        if ((opcode >> 25) & 1) {
            is_cop2_move = false;
            opcode = opcode & 0x1FFFFFF; // Clear op and bit 25 for vector ALU decode
        } else {
            is_cop2_move = true;
        }
    } else {
        // In micro mode, the E bit is bit 31. Clear it so it doesn't mess up exp.
        opcode &= 0x7FFFFFFF;
    }

    uint32_t fmt = (opcode >> 21) & 0x1f;

    // Register extraction
    uint32_t rt = (opcode >> 16) & 0x1f;
    uint32_t fs = (opcode >> 11) & 0x1f;
    uint32_t fd = (opcode >> 6) & 0x1f;

    // Indices for VI registers
    uint32_t It = rt & 0xf;
    uint32_t Is = fs & 0xf;

    uint32_t Id = fd & 0xf;

    // Destination Write Mask (xyzw)
    bool dest_x = (opcode >> 24) & 1;
    bool dest_y = (opcode >> 23) & 1;
    bool dest_z = (opcode >> 22) & 1;
    bool dest_w = (opcode >> 21) & 1;

    // Component broadcast selectors
    uint32_t Fsf = (opcode >> 21) & 3; // S component broadcast: x=0, y=1, z=2, w=3
    uint32_t Ftf = (opcode >> 23) & 3; // T component broadcast: x=0, y=1, z=2, w=3

    // 11-bit sign-extended immediate
    int16_t imm11 = (opcode & 0x7ff);
    if (imm11 & 0x400) {
        imm11 |= 0xf800;
    }

    // 1. GPR <-> COP2 moves (when is_cop2_move is true)
    if (is_cop2_move) {
        switch (fmt) {
            case 0x01: { // QMFC2 rt, fs -> Move VF[fs] to EE GPR[rt]
                if (rt != 0) {
                    ee_gprs[rt].lo = ((uint64_t)state.vf[fs].u[1] << 32) | state.vf[fs].u[0];
                    ee_gprs[rt].hi = ((uint64_t)state.vf[fs].u[3] << 32) | state.vf[fs].u[2];
                }
                break;
            }
            case 0x05: { // QMTC2 rt, fs -> Move EE GPR[rt] to VF[fs]
                if (fs != 0) {
                    state.vf[fs].u[0] = (uint32_t)(ee_gprs[rt].lo & 0xffffffff);
                    state.vf[fs].u[1] = (uint32_t)(ee_gprs[rt].lo >> 32);
                    state.vf[fs].u[2] = (uint32_t)(ee_gprs[rt].hi & 0xffffffff);
                    state.vf[fs].u[3] = (uint32_t)(ee_gprs[rt].hi >> 32);
                }
                break;
            }
            case 0x02: { // CFC2 rt, fs -> Move COP2 Control Reg[fs] to EE GPR[rt]
                if (rt != 0) {
                    int64_t val = 0;
                    if (fs < 16) {
                        val = (int16_t)state.vi[fs];
                    } else if (fs == 16) {
                        val = state.statusflag;
                    } else if (fs == 17) {
                        val = state.macflag;
                    } else if (fs == 18) {
                        val = state.clipflag;
                    } else if (fs == 20) {
                        uint32_t u; std::memcpy(&u, &state.r, 4); val = (int32_t)u;
                    } else if (fs == 21) {
                        uint32_t u; std::memcpy(&u, &state.i, 4); val = (int32_t)u;
                    } else if (fs == 22) {
                        uint32_t u; std::memcpy(&u, &state.q, 4); val = (int32_t)u;
                    } else if (fs == 23) {
                        uint32_t u; std::memcpy(&u, &state.p, 4); val = (int32_t)u;
                    }
                    ee_gprs[rt].lo = (uint64_t)val;
                    ee_gprs[rt].hi = 0;
                }
                break;
            }
            case 0x06: { // CTC2 rt, fs -> Move EE GPR[rt] to COP2 Control Reg[fs]
                uint32_t val_32 = (uint32_t)(ee_gprs[rt].lo & 0xffffffff);
                if (fs < 16) {
                    if (fs != 0) {
                        state.vi[fs] = (int16_t)(val_32 & 0xffff);
                    }
                } else if (fs == 16) {
                    state.statusflag = val_32 & 0xfff;
                } else if (fs == 17) {
                    state.macflag = val_32 & 0xffff;
                } else if (fs == 18) {
                    state.clipflag = val_32 & 0xffffff;
                } else if (fs == 20) {
                    std::memcpy(&state.r, &val_32, 4);
                } else if (fs == 21) {
                    std::memcpy(&state.i, &val_32, 4);
                } else if (fs == 22) {
                    std::memcpy(&state.q, &val_32, 4);
                } else if (fs == 23) {
                    std::memcpy(&state.p, &val_32, 4);
                }
                break;
            }
            default:
                break;
        }
        return;
    }

    // 2. VU macromode instructions (fmt >= 16)
    uint32_t funct = opcode & 0x3f;

    // Check if it is a COP2 SPECIAL2 instruction
    bool is_special2 = (funct & 0x3c) == 0x3c;
    uint32_t idx = 0;
    if (is_special2) {
        idx = (opcode & 0x3) | ((opcode >> 4) & 0x7c);
    }

    // Define float ALU operands (pre-clamped)
    float fs_x = ps2_clamp_operand(state.vf[fs].u[0]);
    float fs_y = ps2_clamp_operand(state.vf[fs].u[1]);
    float fs_z = ps2_clamp_operand(state.vf[fs].u[2]);
    float fs_w = ps2_clamp_operand(state.vf[fs].u[3]);

    float ft_x = ps2_clamp_operand(state.vf[rt].u[0]);
    float ft_y = ps2_clamp_operand(state.vf[rt].u[1]);
    float ft_z = ps2_clamp_operand(state.vf[rt].u[2]);
    float ft_w = ps2_clamp_operand(state.vf[rt].u[3]);

    float acc_x = ps2_clamp_operand(state.acc.u[0]);
    float acc_y = ps2_clamp_operand(state.acc.u[1]);
    float acc_z = ps2_clamp_operand(state.acc.u[2]);
    float acc_w = ps2_clamp_operand(state.acc.u[3]);

    // Helpers for binary MAC updates
    auto perform_binary_op = [&](auto op_func, bool to_acc) {
        VUVector& dst = to_acc ? state.acc : state.vf[fd];
        if (dest_x) dst.u[0] = update_mac_flag(state, 0, op_func(0, fs_x)); else clear_mac_flag(state, 0);
        if (dest_y) dst.u[1] = update_mac_flag(state, 1, op_func(1, fs_y)); else clear_mac_flag(state, 1);
        if (dest_z) dst.u[2] = update_mac_flag(state, 2, op_func(2, fs_z)); else clear_mac_flag(state, 2);
        if (dest_w) dst.u[3] = update_mac_flag(state, 3, op_func(3, fs_w)); else clear_mac_flag(state, 3);
        update_status_flag(state);
    };

    if (!is_special2) {
        // SPECIAL1 instructions (funct < 60)
        switch (funct) {
            // Upper Instructions (Float ALU)
            case 40: { // VADD
                perform_binary_op([&](int c, float s) { return s + (c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w); }, false);
                break;
            }
            case 0: { // VADDx
                perform_binary_op([&](int, float s) { return s + ft_x; }, false);
                break;
            }
            case 1: { // VADDy
                perform_binary_op([&](int, float s) { return s + ft_y; }, false);
                break;
            }
            case 2: { // VADDz
                perform_binary_op([&](int, float s) { return s + ft_z; }, false);
                break;
            }
            case 3: { // VADDw
                perform_binary_op([&](int, float s) { return s + ft_w; }, false);
                break;
            }
            case 34: { // VADDi
                perform_binary_op([&](int, float s) { return s + ps2_clamp_float(state.i); }, false);
                break;
            }
            case 32: { // VADDq
                perform_binary_op([&](int, float s) { return s + ps2_clamp_float(state.q); }, false);
                break;
            }

            case 44: { // VSUB
                perform_binary_op([&](int c, float s) { return s - (c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w); }, false);
                break;
            }
            case 4: { // VSUBx
                perform_binary_op([&](int, float s) { return s - ft_x; }, false);
                break;
            }
            case 5: { // VSUBy
                perform_binary_op([&](int, float s) { return s - ft_y; }, false);
                break;
            }
            case 6: { // VSUBz
                perform_binary_op([&](int, float s) { return s - ft_z; }, false);
                break;
            }
            case 7: { // VSUBw
                perform_binary_op([&](int, float s) { return s - ft_w; }, false);
                break;
            }
            case 38: { // VSUBi
                perform_binary_op([&](int, float s) { return s - ps2_clamp_float(state.i); }, false);
                break;
            }
            case 36: { // VSUBq
                perform_binary_op([&](int, float s) { return s - ps2_clamp_float(state.q); }, false);
                break;
            }

            case 42: { // VMUL
                perform_binary_op([&](int c, float s) { return s * (c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w); }, false);
                break;
            }
            case 24: { // VMULx
                perform_binary_op([&](int, float s) { return s * ft_x; }, false);
                break;
            }
            case 25: { // VMULy
                perform_binary_op([&](int, float s) { return s * ft_y; }, false);
                break;
            }
            case 26: { // VMULz
                perform_binary_op([&](int, float s) { return s * ft_z; }, false);
                break;
            }
            case 27: { // VMULw
                perform_binary_op([&](int, float s) { return s * ft_w; }, false);
                break;
            }
            case 30: { // VMULi
                perform_binary_op([&](int, float s) { return s * ps2_clamp_float(state.i); }, false);
                break;
            }
            case 28: { // VMULq
                perform_binary_op([&](int, float s) { return s * ps2_clamp_float(state.q); }, false);
                break;
            }

            case 43: { // VMAX
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_max(state.vf[fs].u[0], state.vf[rt].u[0]);
                    if (dest_y) state.vf[fd].u[1] = fp_max(state.vf[fs].u[1], state.vf[rt].u[1]);
                    if (dest_z) state.vf[fd].u[2] = fp_max(state.vf[fs].u[2], state.vf[rt].u[2]);
                    if (dest_w) state.vf[fd].u[3] = fp_max(state.vf[fs].u[3], state.vf[rt].u[3]);
                }
                break;
            }
            case 16: { // VMAXx
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_max(state.vf[fs].u[0], state.vf[rt].u[0]);
                    if (dest_y) state.vf[fd].u[1] = fp_max(state.vf[fs].u[1], state.vf[rt].u[0]);
                    if (dest_z) state.vf[fd].u[2] = fp_max(state.vf[fs].u[2], state.vf[rt].u[0]);
                    if (dest_w) state.vf[fd].u[3] = fp_max(state.vf[fs].u[3], state.vf[rt].u[0]);
                }
                break;
            }
            case 17: { // VMAXy
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_max(state.vf[fs].u[0], state.vf[rt].u[1]);
                    if (dest_y) state.vf[fd].u[1] = fp_max(state.vf[fs].u[1], state.vf[rt].u[1]);
                    if (dest_z) state.vf[fd].u[2] = fp_max(state.vf[fs].u[2], state.vf[rt].u[1]);
                    if (dest_w) state.vf[fd].u[3] = fp_max(state.vf[fs].u[3], state.vf[rt].u[1]);
                }
                break;
            }
            case 18: { // VMAXz
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_max(state.vf[fs].u[0], state.vf[rt].u[2]);
                    if (dest_y) state.vf[fd].u[1] = fp_max(state.vf[fs].u[1], state.vf[rt].u[2]);
                    if (dest_z) state.vf[fd].u[2] = fp_max(state.vf[fs].u[2], state.vf[rt].u[2]);
                    if (dest_w) state.vf[fd].u[3] = fp_max(state.vf[fs].u[3], state.vf[rt].u[2]);
                }
                break;
            }
            case 19: { // VMAXw
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_max(state.vf[fs].u[0], state.vf[rt].u[3]);
                    if (dest_y) state.vf[fd].u[1] = fp_max(state.vf[fs].u[1], state.vf[rt].u[3]);
                    if (dest_z) state.vf[fd].u[2] = fp_max(state.vf[fs].u[2], state.vf[rt].u[3]);
                    if (dest_w) state.vf[fd].u[3] = fp_max(state.vf[fs].u[3], state.vf[rt].u[3]);
                }
                break;
            }
            case 29: { // VMAXi
                if (fd != 0) {
                    uint32_t i_u; std::memcpy(&i_u, &state.i, 4);
                    if (dest_x) state.vf[fd].u[0] = fp_max(state.vf[fs].u[0], i_u);
                    if (dest_y) state.vf[fd].u[1] = fp_max(state.vf[fs].u[1], i_u);
                    if (dest_z) state.vf[fd].u[2] = fp_max(state.vf[fs].u[2], i_u);
                    if (dest_w) state.vf[fd].u[3] = fp_max(state.vf[fs].u[3], i_u);
                }
                break;
            }

            case 47: { // VMINI
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_min(state.vf[fs].u[0], state.vf[rt].u[0]);
                    if (dest_y) state.vf[fd].u[1] = fp_min(state.vf[fs].u[1], state.vf[rt].u[1]);
                    if (dest_z) state.vf[fd].u[2] = fp_min(state.vf[fs].u[2], state.vf[rt].u[2]);
                    if (dest_w) state.vf[fd].u[3] = fp_min(state.vf[fs].u[3], state.vf[rt].u[3]);
                }
                break;
            }
            case 20: { // VMINIx
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_min(state.vf[fs].u[0], state.vf[rt].u[0]);
                    if (dest_y) state.vf[fd].u[1] = fp_min(state.vf[fs].u[1], state.vf[rt].u[0]);
                    if (dest_z) state.vf[fd].u[2] = fp_min(state.vf[fs].u[2], state.vf[rt].u[0]);
                    if (dest_w) state.vf[fd].u[3] = fp_min(state.vf[fs].u[3], state.vf[rt].u[0]);
                }
                break;
            }
            case 21: { // VMINIy
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_min(state.vf[fs].u[0], state.vf[rt].u[1]);
                    if (dest_y) state.vf[fd].u[1] = fp_min(state.vf[fs].u[1], state.vf[rt].u[1]);
                    if (dest_z) state.vf[fd].u[2] = fp_min(state.vf[fs].u[2], state.vf[rt].u[1]);
                    if (dest_w) state.vf[fd].u[3] = fp_min(state.vf[fs].u[3], state.vf[rt].u[1]);
                }
                break;
            }
            case 22: { // VMINIz
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_min(state.vf[fs].u[0], state.vf[rt].u[2]);
                    if (dest_y) state.vf[fd].u[1] = fp_min(state.vf[fs].u[1], state.vf[rt].u[2]);
                    if (dest_z) state.vf[fd].u[2] = fp_min(state.vf[fs].u[2], state.vf[rt].u[2]);
                    if (dest_w) state.vf[fd].u[3] = fp_min(state.vf[fs].u[3], state.vf[rt].u[2]);
                }
                break;
            }
            case 23: { // VMINIw
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = fp_min(state.vf[fs].u[0], state.vf[rt].u[3]);
                    if (dest_y) state.vf[fd].u[1] = fp_min(state.vf[fs].u[1], state.vf[rt].u[3]);
                    if (dest_z) state.vf[fd].u[2] = fp_min(state.vf[fs].u[2], state.vf[rt].u[3]);
                    if (dest_w) state.vf[fd].u[3] = fp_min(state.vf[fs].u[3], state.vf[rt].u[3]);
                }
                break;
            }
            case 31: { // VMINIi
                if (fd != 0) {
                    uint32_t i_u; std::memcpy(&i_u, &state.i, 4);
                    if (dest_x) state.vf[fd].u[0] = fp_min(state.vf[fs].u[0], i_u);
                    if (dest_y) state.vf[fd].u[1] = fp_min(state.vf[fs].u[1], i_u);
                    if (dest_z) state.vf[fd].u[2] = fp_min(state.vf[fs].u[2], i_u);
                    if (dest_w) state.vf[fd].u[3] = fp_min(state.vf[fs].u[3], i_u);
                }
                break;
            }

            // Accumulator Chains (VMADD, VMSUB)
            case 41: { // VMADD
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    float ft = c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w;
                    return acc + s * ft;
                }, false);
                break;
            }
            case 8: { // VMADDx
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ft_x;
                }, false);
                break;
            }
            case 9: { // VMADDy
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ft_y;
                }, false);
                break;
            }
            case 10: { // VMADDz
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ft_z;
                }, false);
                break;
            }
            case 11: { // VMADDw
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ft_w;
                }, false);
                break;
            }
            case 35: { // VMADDi
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ps2_clamp_float(state.i);
                }, false);
                break;
            }
            case 33: { // VMADDq
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ps2_clamp_float(state.q);
                }, false);
                break;
            }

            case 45: { // VMSUB
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    float ft = c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w;
                    return acc - s * ft;
                }, false);
                break;
            }
            case 12: { // VMSUBx
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ft_x;
                }, false);
                break;
            }
            case 13: { // VMSUBy
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ft_y;
                }, false);
                break;
            }
            case 14: { // VMSUBz
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ft_z;
                }, false);
                break;
            }
            case 15: { // VMSUBw
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ft_w;
                }, false);
                break;
            }
            case 39: { // VMSUBi
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ps2_clamp_float(state.i);
                }, false);
                break;
            }
            case 37: { // VMSUBq
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ps2_clamp_float(state.q);
                }, false);
                break;
            }
            case 46: { // VOPMSUB
                if (fd != 0) {
                    if (dest_x) state.vf[fd].u[0] = update_mac_flag(state, 0, acc_x - fs_y * ft_z); else clear_mac_flag(state, 0);
                    if (dest_y) state.vf[fd].u[1] = update_mac_flag(state, 1, acc_y - fs_z * ft_x); else clear_mac_flag(state, 1);
                    if (dest_z) state.vf[fd].u[2] = update_mac_flag(state, 2, acc_z - fs_x * ft_y); else clear_mac_flag(state, 2);
                    clear_mac_flag(state, 3);
                }
                update_status_flag(state);
                break;
            }

            // Lower instructions (Integer ALU)
            case 48: { // VIADD
                if (Id != 0) {
                    state.vi[Id] = state.vi[Is] + state.vi[It];
                }
                break;
            }
            case 49: { // VISUB
                if (Id != 0) {
                    state.vi[Id] = state.vi[Is] - state.vi[It];
                }
                break;
            }
            case 50: { // VIADDI
                if (It != 0) {
                    // Sign-extend 5-bit immediate at bits 10-6
                    int16_t imm5 = (opcode >> 6) & 0x1f;
                    if (imm5 & 0x10) imm5 |= 0xffe0;
                    state.vi[It] = state.vi[Is] + imm5;
                }
                break;
            }
            case 52: { // IAND
                if (Id != 0) {
                    state.vi[Id] = state.vi[Is] & state.vi[It];
                }
                break;
            }
            case 53: { // IOR
                if (Id != 0) {
                    state.vi[Id] = state.vi[Is] | state.vi[It];
                }
                break;
            }

            default:
                break;
        }
    } else {
        // SPECIAL2 instructions (funct >= 60, switches on idx)
        switch (idx) {
            // Accumulator-Write Instructions (VMADDA, VMSUBA, VMULA, VADDA, VSUBA)
            case 40: { // VADDA
                perform_binary_op([&](int c, float s) { return s + (c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w); }, true);
                break;
            }
            case 0: { // VADDAx
                perform_binary_op([&](int, float s) { return s + ft_x; }, true);
                break;
            }
            case 1: { // VADDAy
                perform_binary_op([&](int, float s) { return s + ft_y; }, true);
                break;
            }
            case 2: { // VADDAz
                perform_binary_op([&](int, float s) { return s + ft_z; }, true);
                break;
            }
            case 3: { // VADDAw
                perform_binary_op([&](int, float s) { return s + ft_w; }, true);
                break;
            }
            case 34: { // VADDAi
                perform_binary_op([&](int, float s) { return s + ps2_clamp_float(state.i); }, true);
                break;
            }
            case 32: { // VADDAq
                perform_binary_op([&](int, float s) { return s + ps2_clamp_float(state.q); }, true);
                break;
            }

            case 44: { // VSUBA
                perform_binary_op([&](int c, float s) { return s - (c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w); }, true);
                break;
            }
            case 4: { // VSUBAx
                perform_binary_op([&](int, float s) { return s - ft_x; }, true);
                break;
            }
            case 5: { // VSUBAy
                perform_binary_op([&](int, float s) { return s - ft_y; }, true);
                break;
            }
            case 6: { // VSUBAz
                perform_binary_op([&](int, float s) { return s - ft_z; }, true);
                break;
            }
            case 7: { // VSUBAw
                perform_binary_op([&](int, float s) { return s - ft_w; }, true);
                break;
            }
            case 38: { // VSUBAi
                perform_binary_op([&](int, float s) { return s - ps2_clamp_float(state.i); }, true);
                break;
            }
            case 36: { // VSUBAq
                perform_binary_op([&](int, float s) { return s - ps2_clamp_float(state.q); }, true);
                break;
            }

            case 42: { // VMULA
                perform_binary_op([&](int c, float s) { return s * (c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w); }, true);
                break;
            }
            case 24: { // VMULAx
                perform_binary_op([&](int, float s) { return s * ft_x; }, true);
                break;
            }
            case 25: { // VMULAy
                perform_binary_op([&](int, float s) { return s * ft_y; }, true);
                break;
            }
            case 26: { // VMULAz
                perform_binary_op([&](int, float s) { return s * ft_z; }, true);
                break;
            }
            case 27: { // VMULAw
                perform_binary_op([&](int, float s) { return s * ft_w; }, true);
                break;
            }
            case 30: { // VMULAi
                perform_binary_op([&](int, float s) { return s * ps2_clamp_float(state.i); }, true);
                break;
            }
            case 28: { // VMULAq
                perform_binary_op([&](int, float s) { return s * ps2_clamp_float(state.q); }, true);
                break;
            }

            case 41: { // VMADDA
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    float ft = c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w;
                    return acc + s * ft;
                }, true);
                break;
            }
            case 8: { // VMADDAx
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ft_x;
                }, true);
                break;
            }
            case 9: { // VMADDAy
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ft_y;
                }, true);
                break;
            }
            case 10: { // VMADDAz
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ft_z;
                }, true);
                break;
            }
            case 11: { // VMADDAw
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ft_w;
                }, true);
                break;
            }
            case 35: { // VMADDAi
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ps2_clamp_float(state.i);
                }, true);
                break;
            }
            case 33: { // VMADDAq
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc + s * ps2_clamp_float(state.q);
                }, true);
                break;
            }

            case 45: { // VMSUBA
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    float ft = c == 0 ? ft_x : c == 1 ? ft_y : c == 2 ? ft_z : ft_w;
                    return acc - s * ft;
                }, true);
                break;
            }
            case 12: { // VMSUBAx
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ft_x;
                }, true);
                break;
            }
            case 13: { // VMSUBAy
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ft_y;
                }, true);
                break;
            }
            case 14: { // VMSUBAz
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ft_z;
                }, true);
                break;
            }
            case 15: { // VMSUBAw
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ft_w;
                }, true);
                break;
            }
            case 39: { // VMSUBAi
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ps2_clamp_float(state.i);
                }, true);
                break;
            }
            case 37: { // VMSUBAq
                perform_binary_op([&](int c, float s) {
                    float acc = c == 0 ? acc_x : c == 1 ? acc_y : c == 2 ? acc_z : acc_w;
                    return acc - s * ps2_clamp_float(state.q);
                }, true);
                break;
            }

            // Fixed-point conversion (VITOF / VFTOI)
            case 16: { // VITOF0
                if (rt != 0) {
                    if (dest_x) state.vf[rt].f[0] = (float)((int32_t)state.vf[fs].u[0]);
                    if (dest_y) state.vf[rt].f[1] = (float)((int32_t)state.vf[fs].u[1]);
                    if (dest_z) state.vf[rt].f[2] = (float)((int32_t)state.vf[fs].u[2]);
                    if (dest_w) state.vf[rt].f[3] = (float)((int32_t)state.vf[fs].u[3]);
                }
                break;
            }
            case 17: { // VITOF4
                float scale = 1.0f / 16.0f;
                if (rt != 0) {
                    if (dest_x) state.vf[rt].f[0] = (float)((int32_t)state.vf[fs].u[0]) * scale;
                    if (dest_y) state.vf[rt].f[1] = (float)((int32_t)state.vf[fs].u[1]) * scale;
                    if (dest_z) state.vf[rt].f[2] = (float)((int32_t)state.vf[fs].u[2]) * scale;
                    if (dest_w) state.vf[rt].f[3] = (float)((int32_t)state.vf[fs].u[3]) * scale;
                }
                break;
            }
            case 18: { // VITOF12
                float scale = 1.0f / 4096.0f;
                if (rt != 0) {
                    if (dest_x) state.vf[rt].f[0] = (float)((int32_t)state.vf[fs].u[0]) * scale;
                    if (dest_y) state.vf[rt].f[1] = (float)((int32_t)state.vf[fs].u[1]) * scale;
                    if (dest_z) state.vf[rt].f[2] = (float)((int32_t)state.vf[fs].u[2]) * scale;
                    if (dest_w) state.vf[rt].f[3] = (float)((int32_t)state.vf[fs].u[3]) * scale;
                }
                break;
            }
            case 19: { // VITOF15
                float scale = 1.0f / 32768.0f;
                if (rt != 0) {
                    if (dest_x) state.vf[rt].f[0] = (float)((int32_t)state.vf[fs].u[0]) * scale;
                    if (dest_y) state.vf[rt].f[1] = (float)((int32_t)state.vf[fs].u[1]) * scale;
                    if (dest_z) state.vf[rt].f[2] = (float)((int32_t)state.vf[fs].u[2]) * scale;
                    if (dest_w) state.vf[rt].f[3] = (float)((int32_t)state.vf[fs].u[3]) * scale;
                }
                break;
            }

            case 20: { // VFTOI0
                auto ftoi_func = [](uint32_t uval, float scale) -> uint32_t {
                    float f; std::memcpy(&f, &uval, 4);
                    f *= scale;
                    uint32_t u; std::memcpy(&u, &f, 4);
                    if ((u & 0x7f800000) >= 0x4f000000) {
                        return (u & 0x80000000) ? 0x80000000 : 0x7fffffff;
                    }
                    return (uint32_t)(int32_t)f;
                };
                if (rt != 0) {
                    if (dest_x) state.vf[rt].u[0] = ftoi_func(state.vf[fs].u[0], 1.0f);
                    if (dest_y) state.vf[rt].u[1] = ftoi_func(state.vf[fs].u[1], 1.0f);
                    if (dest_z) state.vf[rt].u[2] = ftoi_func(state.vf[fs].u[2], 1.0f);
                    if (dest_w) state.vf[rt].u[3] = ftoi_func(state.vf[fs].u[3], 1.0f);
                }
                break;
            }
            case 21: { // VFTOI4
                auto ftoi_func = [](uint32_t uval, float scale) -> uint32_t {
                    float f; std::memcpy(&f, &uval, 4);
                    f *= scale;
                    uint32_t u; std::memcpy(&u, &f, 4);
                    if ((u & 0x7f800000) >= 0x4f000000) {
                        return (u & 0x80000000) ? 0x80000000 : 0x7fffffff;
                    }
                    return (uint32_t)(int32_t)f;
                };
                if (rt != 0) {
                    if (dest_x) state.vf[rt].u[0] = ftoi_func(state.vf[fs].u[0], 16.0f);
                    if (dest_y) state.vf[rt].u[1] = ftoi_func(state.vf[fs].u[1], 16.0f);
                    if (dest_z) state.vf[rt].u[2] = ftoi_func(state.vf[fs].u[2], 16.0f);
                    if (dest_w) state.vf[rt].u[3] = ftoi_func(state.vf[fs].u[3], 16.0f);
                }
                break;
            }
            case 22: { // VFTOI12
                auto ftoi_func = [](uint32_t uval, float scale) -> uint32_t {
                    float f; std::memcpy(&f, &uval, 4);
                    f *= scale;
                    uint32_t u; std::memcpy(&u, &f, 4);
                    if ((u & 0x7f800000) >= 0x4f000000) {
                        return (u & 0x80000000) ? 0x80000000 : 0x7fffffff;
                    }
                    return (uint32_t)(int32_t)f;
                };
                if (rt != 0) {
                    if (dest_x) state.vf[rt].u[0] = ftoi_func(state.vf[fs].u[0], 4096.0f);
                    if (dest_y) state.vf[rt].u[1] = ftoi_func(state.vf[fs].u[1], 4096.0f);
                    if (dest_z) state.vf[rt].u[2] = ftoi_func(state.vf[fs].u[2], 4096.0f);
                    if (dest_w) state.vf[rt].u[3] = ftoi_func(state.vf[fs].u[3], 4096.0f);
                }
                break;
            }
            case 23: { // VFTOI15
                auto ftoi_func = [](uint32_t uval, float scale) -> uint32_t {
                    float f; std::memcpy(&f, &uval, 4);
                    f *= scale;
                    uint32_t u; std::memcpy(&u, &f, 4);
                    if ((u & 0x7f800000) >= 0x4f000000) {
                        return (u & 0x80000000) ? 0x80000000 : 0x7fffffff;
                    }
                    return (uint32_t)(int32_t)f;
                };
                if (rt != 0) {
                    if (dest_x) state.vf[rt].u[0] = ftoi_func(state.vf[fs].u[0], 32768.0f);
                    if (dest_y) state.vf[rt].u[1] = ftoi_func(state.vf[fs].u[1], 32768.0f);
                    if (dest_z) state.vf[rt].u[2] = ftoi_func(state.vf[fs].u[2], 32768.0f);
                    if (dest_w) state.vf[rt].u[3] = ftoi_func(state.vf[fs].u[3], 32768.0f);
                }
                break;
            }

            // Other EFU/Moves
            case 29: { // VABS
                if (rt != 0) {
                    if (dest_x) state.vf[rt].u[0] = state.vf[fs].u[0] & 0x7fffffff;
                    if (dest_y) state.vf[rt].u[1] = state.vf[fs].u[1] & 0x7fffffff;
                    if (dest_z) state.vf[rt].u[2] = state.vf[fs].u[2] & 0x7fffffff;
                    if (dest_w) state.vf[rt].u[3] = state.vf[fs].u[3] & 0x7fffffff;
                }
                break;
            }
            case 31: { // VCLIPw
                uint32_t val_u = state.vf[rt].u[3]; // Ft.w
                int32_t value = (val_u & 0x7f800000) ? (val_u & 0x7fffffff) : 0x007fffff;
                int32_t s_x_pos = (int32_t)state.vf[fs].u[0];
                int32_t s_x_neg = (int32_t)(state.vf[fs].u[0] ^ 0x80000000);
                int32_t s_y_pos = (int32_t)state.vf[fs].u[1];
                int32_t s_y_neg = (int32_t)(state.vf[fs].u[1] ^ 0x80000000);
                int32_t s_z_pos = (int32_t)state.vf[fs].u[2];
                int32_t s_z_neg = (int32_t)(state.vf[fs].u[2] ^ 0x80000000);

                state.clipflag <<= 6;
                if (s_x_pos > value) state.clipflag |= 0x01;
                if (s_x_neg > value) state.clipflag |= 0x02;
                if (s_y_pos > value) state.clipflag |= 0x04;
                if (s_y_neg > value) state.clipflag |= 0x08;
                if (s_z_pos > value) state.clipflag |= 0x10;
                if (s_z_neg > value) state.clipflag |= 0x20;
                state.clipflag &= 0xffffff;
                break;
            }
            case 46: { // VOPMULA
                state.acc.u[0] = update_mac_flag(state, 0, fs_y * ft_z);
                state.acc.u[1] = update_mac_flag(state, 1, fs_z * ft_x);
                state.acc.u[2] = update_mac_flag(state, 2, fs_x * ft_y);
                update_status_flag(state);
                break;
            }
            case 48: { // VMOVE
                if (rt != 0) {
                    if (dest_x) state.vf[rt].u[0] = state.vf[fs].u[0];
                    if (dest_y) state.vf[rt].u[1] = state.vf[fs].u[1];
                    if (dest_z) state.vf[rt].u[2] = state.vf[fs].u[2];
                    if (dest_w) state.vf[rt].u[3] = state.vf[fs].u[3];
                }
                break;
            }
            case 49: { // VMR32
                if (rt != 0) {
                    uint32_t tx = state.vf[fs].u[0];
                    if (dest_x) state.vf[rt].u[0] = state.vf[fs].u[1];
                    if (dest_y) state.vf[rt].u[1] = state.vf[fs].u[2];
                    if (dest_z) state.vf[rt].u[2] = state.vf[fs].u[3];
                    if (dest_w) state.vf[rt].u[3] = tx;
                }
                break;
            }

            // Elementary Function Unit (VDIV, VSQRT, VRSQRT)
            case 56: { // VDIV
                float ft = ps2_clamp_operand(state.vf[rt].u[Ftf]);
                float fs_val = ps2_clamp_operand(state.vf[fs].u[Fsf]);
                uint32_t efu_flags = 0;

                if (ft == 0.0f) {
                    if (fs_val == 0.0f) {
                        efu_flags |= 0x10; // Invalid
                    } else {
                        efu_flags |= 0x20; // Division-by-zero
                    }
                    uint32_t sign_diff = (state.vf[rt].u[Ftf] & 0x80000000) ^ (state.vf[fs].u[Fsf] & 0x80000000);
                    uint32_t q_val = sign_diff ? 0xFF7FFFFF : 0x7F7FFFFF;
                    std::memcpy(&state.q, &q_val, 4);
                } else {
                    float res = fs_val / ft;
                    state.q = ps2_clamp_float(res);
                }
                update_status_flag_efu(state, efu_flags);
                break;
            }
            case 57: { // VSQRT
                float ft = ps2_clamp_operand(state.vf[rt].u[Ftf]);
                uint32_t efu_flags = 0;

                if (ft < 0.0f) {
                    efu_flags |= 0x10; // Invalid
                }
                float res = std::sqrt(std::abs(ft));
                state.q = ps2_clamp_float(res);
                update_status_flag_efu(state, efu_flags);
                break;
            }
            case 58: { // VRSQRT
                float ft = ps2_clamp_operand(state.vf[rt].u[Ftf]);
                float fs_val = ps2_clamp_operand(state.vf[fs].u[Fsf]);
                uint32_t efu_flags = 0;

                if (ft == 0.0f) {
                    efu_flags |= 0x20; // Division-by-zero
                    if (fs_val != 0.0f) {
                        uint32_t sign_diff = (state.vf[rt].u[Ftf] & 0x80000000) ^ (state.vf[fs].u[Fsf] & 0x80000000);
                        uint32_t q_val = sign_diff ? 0xFF7FFFFF : 0x7F7FFFFF;
                        std::memcpy(&state.q, &q_val, 4);
                    } else {
                        uint32_t sign_diff = (state.vf[rt].u[Ftf] & 0x80000000) ^ (state.vf[fs].u[Fsf] & 0x80000000);
                        uint32_t q_val = sign_diff ? 0x80000000 : 0x00000000;
                        std::memcpy(&state.q, &q_val, 4);
                        efu_flags |= 0x10; // Invalid
                    }
                } else {
                    if (ft < 0.0f) {
                        efu_flags |= 0x10; // Invalid
                    }
                    float temp = std::sqrt(std::abs(ft));
                    float res = fs_val / temp;
                    state.q = ps2_clamp_float(res);
                }
                update_status_flag_efu(state, efu_flags);
                break;
            }

            case 60: { // VMTIR -> Move from VF to Integer register
                if (It != 0) {
                    uint32_t raw_val = state.vf[fs].u[Fsf];
                    state.vi[It] = (int16_t)(raw_val & 0xffff);
                }
                break;
            }
            case 61: { // VMFIR -> Move from Integer register to VF
                if (rt != 0) {
                    int32_t val = (int32_t)state.vi[Is];
                    uint32_t val_u; std::memcpy(&val_u, &val, 4);
                    if (dest_x) state.vf[rt].u[0] = val_u;
                    if (dest_y) state.vf[rt].u[1] = val_u;
                    if (dest_z) state.vf[rt].u[2] = val_u;
                    if (dest_w) state.vf[rt].u[3] = val_u;
                }
                break;
            }
            case 62: { // VILWR
                if (It != 0) {
                    uint32_t addr = (uint32_t)state.vi[Is] * 16;
                    uint16_t* ptr = (uint16_t*)&state.mem[addr & 0x3fff];
                    if (dest_x) state.vi[It] = (int16_t)ptr[0];
                    if (dest_y) state.vi[It] = (int16_t)ptr[2];
                    if (dest_z) state.vi[It] = (int16_t)ptr[4];
                    if (dest_w) state.vi[It] = (int16_t)ptr[6];
                }
                break;
            }
            case 63: { // VISWR
                uint32_t addr = (uint32_t)state.vi[Is] * 16;
                uint16_t* ptr = (uint16_t*)&state.mem[addr & 0x3fff];
                if (dest_x) { ptr[0] = (uint16_t)state.vi[It]; ptr[1] = 0; }
                if (dest_y) { ptr[2] = (uint16_t)state.vi[It]; ptr[3] = 0; }
                if (dest_z) { ptr[4] = (uint16_t)state.vi[It]; ptr[5] = 0; }
                if (dest_w) { ptr[6] = (uint16_t)state.vi[It]; ptr[7] = 0; }
                break;
            }

            // LFSR Random Generator
            case 64: { // VRNEXT
                if (rt != 0) {
                    advance_lfsr(state);
                    uint32_t r_u; std::memcpy(&r_u, &state.r, 4);
                    if (dest_x) state.vf[rt].u[0] = r_u;
                    if (dest_y) state.vf[rt].u[1] = r_u;
                    if (dest_z) state.vf[rt].u[2] = r_u;
                    if (dest_w) state.vf[rt].u[3] = r_u;
                }
                break;
            }
            case 65: { // VRGET
                if (rt != 0) {
                    uint32_t r_u; std::memcpy(&r_u, &state.r, 4);
                    if (dest_x) state.vf[rt].u[0] = r_u;
                    if (dest_y) state.vf[rt].u[1] = r_u;
                    if (dest_z) state.vf[rt].u[2] = r_u;
                    if (dest_w) state.vf[rt].u[3] = r_u;
                }
                break;
            }
            case 66: { // VRINIT
                state.r = ps2_clamp_operand(0x3f800000 | (state.vf[fs].u[Fsf] & 0x007fffff));
                break;
            }
            case 67: { // VRXOR
                uint32_t r_u; std::memcpy(&r_u, &state.r, 4);
                state.r = ps2_clamp_operand(0x3f800000 | ((r_u ^ state.vf[fs].u[Fsf]) & 0x007fffff));
                break;
            }

            // Load and Store (VLQ/VSQ/VILW/VISW)
            case 52: { // VLQI
                if (rt != 0) { // Ft
                    uint32_t addr = (uint32_t)state.vi[Is] * 16;
                    uint32_t* ptr = (uint32_t*)&state.mem[addr & 0x3fff];
                    if (dest_x) state.vf[rt].u[0] = ptr[0];
                    if (dest_y) state.vf[rt].u[1] = ptr[1];
                    if (dest_z) state.vf[rt].u[2] = ptr[2];
                    if (dest_w) state.vf[rt].u[3] = ptr[3];
                }
                if (fs != 0) { // Fs
                    state.vi[Is]++;
                }
                break;
            }
            case 53: { // VSQI
                uint32_t addr = (uint32_t)state.vi[It] * 16;
                uint32_t* ptr = (uint32_t*)&state.mem[addr & 0x3fff];
                if (dest_x) ptr[0] = state.vf[fs].u[0];
                if (dest_y) ptr[1] = state.vf[fs].u[1];
                if (dest_z) ptr[2] = state.vf[fs].u[2];
                if (dest_w) ptr[3] = state.vf[fs].u[3];
                if (rt != 0) { // Ft
                    state.vi[It]++;
                }
                break;
            }
            case 54: { // VLQD
                if (Is != 0) {
                    state.vi[Is]--;
                }
                if (rt != 0) { // Ft
                    uint32_t addr = (uint32_t)state.vi[Is] * 16;
                    uint32_t* ptr = (uint32_t*)&state.mem[addr & 0x3fff];
                    if (dest_x) state.vf[rt].u[0] = ptr[0];
                    if (dest_y) state.vf[rt].u[1] = ptr[1];
                    if (dest_z) state.vf[rt].u[2] = ptr[2];
                    if (dest_w) state.vf[rt].u[3] = ptr[3];
                }
                break;
            }
            case 55: { // VSQD
                if (It != 0) {
                    state.vi[It]--;
                }
                uint32_t addr = (uint32_t)state.vi[It] * 16;
                uint32_t* ptr = (uint32_t*)&state.mem[addr & 0x3fff];
                if (dest_x) ptr[0] = state.vf[fs].u[0];
                if (dest_y) ptr[1] = state.vf[fs].u[1];
                if (dest_z) ptr[2] = state.vf[fs].u[2];
                if (dest_w) ptr[3] = state.vf[fs].u[3];
                break;
            }

            default:
                break;
        }
    }

    // Always enforce hardwired constraints:
    // vf00 = [0, 0, 0, 1]
    state.vf[0].f[0] = 0.0f;
    state.vf[0].f[1] = 0.0f;
    state.vf[0].f[2] = 0.0f;
    state.vf[0].f[3] = 1.0f;
    // vi00 = 0
    state.vi[0] = 0;
}

// -------------------------------------------------------------------------
//  VU Micro-Mode Execution Engine
// -------------------------------------------------------------------------

static void execute_lower2_instruction(VUState& state, uint32_t instr, uint32_t current_pc, uint32_t& branch_target, bool& branch_taken) {
    uint32_t op = (instr >> 25) & 0x7F;
    uint32_t Is = (instr >> 11) & 0xf;
    uint32_t It = (instr >> 16) & 0xf;
    uint32_t fs = (instr >> 11) & 0x1f;
    uint32_t rt = (instr >> 16) & 0x1f;
    
    
    // Destination Write Mask (xyzw) for load/store
    bool dest_x = (instr >> 24) & 1;
    bool dest_y = (instr >> 23) & 1;
    bool dest_z = (instr >> 22) & 1;
    bool dest_w = (instr >> 21) & 1;

    int16_t imm11 = instr & 0x7ff;
    if (imm11 & 0x400) imm11 |= 0xf800; // Sign extend

    int16_t imm15 = instr & 0x7fff;
    if (imm15 & 0x4000) imm15 |= 0x8000;

    int32_t imm24 = instr & 0xffffff;
    if (imm24 & 0x800000) imm24 |= 0xff000000;

    switch (op) {
        case 0x00: { // LQ ft, imm11(is)
            uint32_t addr = (state.vi[Is] + imm11) & 0x3ff; // 10-bit wrap
            uint32_t byte_addr = addr * 16;
            if (rt != 0 && byte_addr < 16384) {
                float* mem_f = (float*)&state.mem[byte_addr];
                if (dest_x) state.vf[rt].f[0] = mem_f[0];
                if (dest_y) state.vf[rt].f[1] = mem_f[1];
                if (dest_z) state.vf[rt].f[2] = mem_f[2];
                if (dest_w) state.vf[rt].f[3] = mem_f[3];

            }
            break;
        }
        case 0x01: { // SQ fs, imm11(it) -> WAIT! syntax is SQ fs, imm11(is) where fs is in the ft field?
            // Actually 'ft' field in instruction format is the source register for store.
            uint32_t addr = (state.vi[It] + imm11) & 0x3ff;
            uint32_t byte_addr = addr * 16;
            if (byte_addr < 16384) {
                float* mem_f = (float*)&state.mem[byte_addr];
                if (dest_x) mem_f[0] = state.vf[fs].f[0];
                if (dest_y) mem_f[1] = state.vf[fs].f[1];
                if (dest_z) mem_f[2] = state.vf[fs].f[2];
                if (dest_w) mem_f[3] = state.vf[fs].f[3];
            }
            break;
        }
        case 0x04: { // ILW it, imm11(is)
            uint32_t addr = (state.vi[Is] + imm11) & 0x3ff;
            uint32_t byte_addr = addr * 16;
            if (It != 0 && byte_addr < 16384) {
                uint32_t* mem_u = (uint32_t*)&state.mem[byte_addr];
                uint32_t val = 0;
                if (dest_x) val = mem_u[0];
                else if (dest_y) val = mem_u[1];
                else if (dest_z) val = mem_u[2];
                else if (dest_w) val = mem_u[3];
                state.vi[It] = (int16_t)(val & 0xffff);
                
            }
            break;
        }
        case 0x05: { // ISW it, imm11(is)
            uint32_t addr = (state.vi[Is] + imm11) & 0x3ff;
            uint32_t byte_addr = addr * 16;
            if (byte_addr < 16384) {
                uint32_t* mem_u = (uint32_t*)&state.mem[byte_addr];
                uint32_t val = (uint32_t)(uint16_t)state.vi[It];
                if (dest_x) mem_u[0] = val;
                if (dest_y) mem_u[1] = val;
                if (dest_z) mem_u[2] = val;
                if (dest_w) mem_u[3] = val;
            }
            break;
        }
        case 0x08: { // IADDIU it, is, imm15
            if (It != 0) {
                const int32_t imm15_val = static_cast<int32_t>((instr & 0x7ff) | (((instr >> 21) & 0xf) << 11));  // unsigned
                state.vi[It] = static_cast<int16_t>(state.vi[Is] + imm15_val);
            }
            break;
        }
        case 0x09: { // ISUBIU it, is, imm15
            if (It != 0) {
                const int32_t imm15_val = static_cast<int32_t>((instr & 0x7ff) | (((instr >> 21) & 0xf) << 11));  // unsigned
                state.vi[It] = static_cast<int16_t>(state.vi[Is] - imm15_val);
            }
            break;
        }

        // Status flag tests (12-bit immediate: bit 11 from bit 21), on the flag as it was 4
        // instructions ago (the FMAC's latency, as the MAC flags).
        case 0x14: { // FSEQ it, imm12
            const uint32_t it = (instr >> 16) & 0xf, imm = ((instr >> 10) & 0x800) | (instr & 0x7ff);
            if (it != 0) state.vi[it] = (state.status_visible & 0xfff) == imm ? 1 : 0;
            break;
        }
        case 0x15: { // FSSET imm12 (the sticky bits)
            const uint32_t imm = ((instr >> 10) & 0x800) | (instr & 0x7ff);
            state.statusflag = (state.statusflag & 0x3f) | (imm & 0xfc0);
            break;
        }
        case 0x16: { // FSAND it, imm12
            const uint32_t it = (instr >> 16) & 0xf, imm = ((instr >> 10) & 0x800) | (instr & 0x7ff);
            if (it != 0) state.vi[it] = static_cast<int16_t>(state.status_visible & imm);
            break;
        }
        case 0x17: { // FSOR it, imm12
            const uint32_t it = (instr >> 16) & 0xf, imm = ((instr >> 10) & 0x800) | (instr & 0x7ff);
            if (it != 0) state.vi[it] = static_cast<int16_t>((state.status_visible & 0xfff) | imm);
            break;
        }
        // MAC flag tests. The MAC flag register reflects an FMAC instruction 4
        // instructions after it issues; execute_microprogram keeps that delayed
        // value in state.mac_visible. (Used e.g. by the VU0 Euler-matrix program
        // at micro address 0x1a3 to skip zero angles.)
        case 0x18: { // FMEQ it, is
            const uint32_t it = (instr >> 16) & 0xf, is = (instr >> 11) & 0xf;
            if (it != 0) state.vi[it] = ((static_cast<uint32_t>(static_cast<uint16_t>(state.vi[is])) & 0xffff) == (state.mac_visible & 0xffff)) ? 1 : 0;
            break;
        }
        case 0x1A: { // FMAND it, is
            const uint32_t it = (instr >> 16) & 0xf, is = (instr >> 11) & 0xf;
            if (it != 0) state.vi[it] = static_cast<int16_t>(static_cast<uint16_t>(state.vi[is]) & (state.mac_visible & 0xffff));
            break;
        }
        case 0x1B: { // FMOR it, is
            const uint32_t it = (instr >> 16) & 0xf, is = (instr >> 11) & 0xf;
            if (it != 0) state.vi[it] = static_cast<int16_t>(static_cast<uint16_t>(state.vi[is]) | (state.mac_visible & 0xffff));
            break;
        }
        // Clip flag tests, on the flag as it was 4 instructions ago (CLIP's latency): FCGET its
        // low 12 bits, FCEQ / FCAND / FCOR (vi1, the 24-bit immediate) equal, any of the masked
        // flags set, all the flags outside the mask set; FCSET writes it.
        case 0x1c: { // FCGET it
            const uint32_t it = (instr >> 16) & 0xf;
            if (it != 0) state.vi[it] = static_cast<int16_t>(state.clip_visible & 0xfff);
            break;
        }
        case 0x10: { // FCEQ vi1, imm24
            state.vi[1] = (state.clip_visible & 0xffffff) == (instr & 0xffffff) ? 1 : 0;
            break;
        }
        case 0x12: { // FCAND vi1, imm24
            state.vi[1] = (state.clip_visible & (instr & 0xffffff)) != 0 ? 1 : 0;
            break;
        }
        case 0x13: { // FCOR vi1, imm24
            state.vi[1] = ((state.clip_visible | (instr & 0xffffff)) & 0xffffff) == 0xffffff ? 1 : 0;
            break;
        }
        case 0x11: { // FCSET imm24
            state.clipflag = instr & 0xffffff;
            break;
        }
        case 0x20: { // B
            branch_taken = true;
            branch_target = current_pc + 1 + imm11;
            break;
        }
        case 0x21: { // BAL
            if (It != 0) state.vi[It] = current_pc + 2;
            branch_taken = true;
            branch_target = current_pc + 1 + imm11;
            break;
        }
        case 0x24: { // JR
            branch_taken = true;
            branch_target = state.vi[Is];
            break;
        }
        case 0x25: { // JALR
            if (It != 0) state.vi[It] = current_pc + 2;
            branch_taken = true;
            branch_target = state.vi[Is];
            break;
        }
        case 0x28: { // IBEQ
            if (state.vi[Is] == state.vi[It]) {
                branch_taken = true;
                branch_target = current_pc + 1 + imm11;
            }
            break;
        }
        case 0x29: { // IBNE
            if (state.vi[Is] != state.vi[It]) {
                branch_taken = true;
                branch_target = current_pc + 1 + imm11;
            }
            break;
        }
        case 0x2C: { // IBLTZ
            if (state.vi[Is] < 0) {
                branch_taken = true;
                branch_target = current_pc + 1 + imm11;
            }
            break;
        }
        case 0x2D: { // IBGTZ
            if (state.vi[Is] > 0) {
                branch_taken = true;
                branch_target = current_pc + 1 + imm11;
            }
            break;
        }
        case 0x2E: { // IBLEZ
            if (state.vi[Is] <= 0) {
                branch_taken = true;
                branch_target = current_pc + 1 + imm11;
            }
            break;
        }
        case 0x2F: { // IBGEZ
            if (state.vi[Is] >= 0) {
                branch_taken = true;
                branch_target = current_pc + 1 + imm11;
            }
            break;
        }
        default:
            break;
    }
}

void VuMicroRun::start(VUState& state, const uint64_t* micromem, size_t micromem_size, uint32_t start_pc, VuNativeStep native) {
    st_ = &state;
    mem_ = micromem;
    native_ = native;
    p_ = VuPipe{};
    p_.size = micromem_size;
    p_.pc = start_pc;
    p_.halted = false;
    state.vf[0].u[0] = state.vf[0].u[1] = state.vf[0].u[2] = 0;  // (hardwired: vf0 = (0, 0, 0, 1), vi0 = 0)
    state.vf[0].f[3] = 1.0f;
    state.vi[0] = 0;
    for (uint32_t& m : p_.mac_ring) m = state.macflag;
    for (uint32_t& m : p_.status_ring) m = state.statusflag;
    for (uint32_t& m : p_.clip_ring) m = state.clipflag;
}

bool VuMicroRun::step() {
    if (p_.halted || p_.pc >= p_.size) return false;
    if (native_ != nullptr) return native_(*st_, p_);
    VUState& state = *st_;
    const uint64_t* const micromem = mem_;
    uint32_t& pc = p_.pc;
    bool& branch_pending = p_.branch_pending;
    uint32_t& branch_target = p_.branch_target;
    bool& halt_pending = p_.halt_pending;
    int& halt_delay = p_.halt_delay;
    bool& halted = p_.halted;
    uint32_t* const mac_ring = p_.mac_ring;
    uint64_t& issued = p_.issued;
    bool& q_pending = p_.q_pending;
    float& q_next = p_.q_next;
    uint64_t& q_ready = p_.q_ready;
    int& kick_delay = p_.kick_delay;
    uint32_t& kick_addr = p_.kick_addr;
    auto do_kick = [&state](uint32_t addr) {
        if (state.xgkick_hook) state.xgkick_hook(addr);
        else vu_execute_xgkick(state, addr, state.xgkick_primitives);
    };
        state.mac_visible = mac_ring[issued & 3];  // value after instruction (issued - 4)
        state.status_visible = p_.status_ring[issued & 3];
        state.clip_visible = p_.clip_ring[issued & 3];
        if (q_pending && issued >= q_ready) {
            state.q = q_next;
            q_pending = false;
        }
        uint64_t instr = micromem[pc];
        static const bool pctrace = std::getenv("RAC1_VU_PCTRACE") != nullptr;
        if (pctrace)
            std::fprintf(stderr, "[vu] %03x vi1 %x vi2 %x vi9 %x vi10 %x vi11 %x vi12 %x vi13 %x vi15 %x\n", pc, static_cast<uint16_t>(state.vi[1]),
                         static_cast<uint16_t>(state.vi[2]), static_cast<uint16_t>(state.vi[9]), static_cast<uint16_t>(state.vi[10]),
                         static_cast<uint16_t>(state.vi[11]), static_cast<uint16_t>(state.vi[12]), static_cast<uint16_t>(state.vi[13]),
                         static_cast<uint16_t>(state.vi[15]));
        uint32_t upper = (uint32_t)(instr >> 32);
        uint32_t lower = (uint32_t)(instr & 0xFFFFFFFF);
        const uint32_t lower_special = (lower & 0x3) | ((lower >> 4) & 0x7c);
        const bool lower_is_efu = (upper & 0x80000000) == 0 && (lower & 0x80000000) != 0 && (lower & 0x3c) == 0x3c &&
                                  (lower_special == 0x38 || lower_special == 0x39 || lower_special == 0x3a || lower_special == 0x3b);
        if (lower_is_efu && q_pending) {  // the EFU busy: wait for the previous result
            state.q = q_next;
            q_pending = false;
        }
        const float q_before = state.q;
        // upper and lower both read the registers as they were before this instruction
        const std::array<VUVector, 32> vf_before = state.vf;
        // The integer branch hazard: a conditional branch (IBEQ ... IBGEZ) right after an instruction that wrote an integer register it
        // tests sees the register's value from before that write. Established by the world's shrub program (0x103570 ... the strip loop's
        // `mtir vi4, vf12w` / `ibltz vi4`: with the new value the last vertex of every strip is never written). Writers by class (RAC1_VU_HZ=<mask>
        // changes the set, testing): 1 integer ALU (IADD / ISUB / IADDI / IAND / IOR / IADDIU / ISUBIU: assumed, no program of the game has such a
        // pair), 2 MTIR (needed), 4 ILW / ILWR, 8 the pointer update of LQI / SQI / LQD / SQD, 16 the flag reads (FSEQ ... FCGET: 28 of them sit
        // right before a branch in the tfrag / tie programs, which need the new value). JR / JALR read the new value (the moby code: `iaddiu vi1,
        // vi3, 5` / `jalr vi15, vi1`).
        static const uint32_t hz_mask = std::getenv("RAC1_VU_HZ") != nullptr ? static_cast<uint32_t>(std::strtoul(std::getenv("RAC1_VU_HZ"), nullptr, 0)) : 3;
        const std::array<int16_t, 16> vi_before = state.vi;
        int hz_swapped = -1;
        if (hz_mask != 0 && p_.vi_hz_reg >= 0 && (upper & 0x80000000) == 0 && (lower & 0x80000000) == 0) {
            const uint32_t bop = (lower >> 25) & 0x7f;
            if (bop >= 0x28 && bop <= 0x2f) {
                const int bs = static_cast<int>((lower >> 11) & 15), bt = static_cast<int>((lower >> 16) & 15);
                if (bs == p_.vi_hz_reg || ((bop == 0x28 || bop == 0x29) && bt == p_.vi_hz_reg)) {
                    hz_swapped = p_.vi_hz_reg;
                    state.vi[static_cast<size_t>(hz_swapped)] = p_.vi_hz_old;
                }
            }
        }

        bool branch_taken_this_cycle = false;
        uint32_t new_branch_target = 0;

        // Check for E-bit (halts after the next instruction)
        if ((upper & 0x40000000) != 0 && !halt_pending) {
            halt_pending = true;
            halt_delay = 2;
        }

                        bool update_i_reg = false;
        uint32_t new_i_reg = 0;

        // I-bit check (LOI)
        if ((upper & 0x80000000) != 0) {
            // Lower 32 bits are immediate float for I register
            update_i_reg = true;
            new_i_reg = lower;
        } else {
            // Execute lower instruction

            if ((lower & 0x80000000) != 0 && (lower & 0x3c) == 0x3c) {  // lower special2 (funct 0x3c..0x3f); special1 (IADD, IAND...) below
                uint32_t op_special = (lower & 0x3) | ((lower >> 4) & 0x7c);
                if (op_special == 0x31) { // MR32 (rotated from a copy: fs may be ft)
                    const uint32_t fs = (lower >> 11) & 0x1f, ft = (lower >> 16) & 0x1f;
                    const VUVector src = state.vf[fs];
                    if (ft != 0)
                        for (int l = 0; l < 4; ++l)
                            if (((lower >> (24 - l)) & 1) != 0) state.vf[ft].u[l] = src.u[(l + 1) & 3];
                } else if (op_special == 0x6C) { // XGKICK
                    uint32_t Is = (lower >> 11) & 0xf;
                    if (kick_delay != 0) do_kick(kick_addr);
                    kick_addr = static_cast<uint16_t>(state.vi[Is]);
                    kick_delay = 2;
                } else if (op_special == 0x68) { // XTOP it: VIF1's TOP
                    const uint32_t It = (lower >> 16) & 0xf;
                    if (It != 0) state.vi[It] = static_cast<int16_t>(state.xtop & 0x3ff);
                } else if (op_special >= 0x3C && op_special <= 0x43) {
                    uint32_t fs = (lower >> 11) & 0x1f;
                    uint32_t rt = (lower >> 16) & 0x1f;
                    uint32_t It = rt & 0xf;
                    uint32_t Is = fs & 0xf;
                    uint32_t Fsf = (lower >> 21) & 3;
                    bool dest_x = (lower >> 24) & 1;
                    bool dest_y = (lower >> 23) & 1;
                    bool dest_z = (lower >> 22) & 1;
                    bool dest_w = (lower >> 21) & 1;
                    
                    switch (op_special) {
                        case 0x3C: { // MTIR
                            if (It != 0) {
                                uint32_t raw_val = state.vf[fs].u[Fsf];
                                state.vi[It] = (int16_t)(raw_val & 0xffff);
                            }
                            break;
                        }
                        case 0x3D: { // MFIR
                            if (rt != 0) {
                                int32_t val = (int32_t)state.vi[Is];
                                uint32_t val_u; std::memcpy(&val_u, &val, 4);
                                if (dest_x) state.vf[rt].u[0] = val_u;
                                if (dest_y) state.vf[rt].u[1] = val_u;
                                if (dest_z) state.vf[rt].u[2] = val_u;
                                if (dest_w) state.vf[rt].u[3] = val_u;
                            }
                            break;
                        }
                        case 0x3E: { // ILWR
                            if (It != 0) {
                                uint32_t addr = (uint32_t)state.vi[Is] * 16;
                                uint16_t* ptr = (uint16_t*)&state.mem[addr & 0x3fff];
                                if (dest_x) state.vi[It] = (int16_t)ptr[0];
                                if (dest_y) state.vi[It] = (int16_t)ptr[2];
                                if (dest_z) state.vi[It] = (int16_t)ptr[4];
                                if (dest_w) state.vi[It] = (int16_t)ptr[6];
                            }
                            break;
                        }
                        case 0x3F: { // ISWR
                            uint32_t addr = (uint32_t)state.vi[Is] * 16;
                            uint16_t* ptr = (uint16_t*)&state.mem[addr & 0x3fff];
                            if (dest_x) { ptr[0] = (uint16_t)state.vi[It]; ptr[1] = 0; }
                            if (dest_y) { ptr[2] = (uint16_t)state.vi[It]; ptr[3] = 0; }
                            if (dest_z) { ptr[4] = (uint16_t)state.vi[It]; ptr[5] = 0; }
                            if (dest_w) { ptr[6] = (uint16_t)state.vi[It]; ptr[7] = 0; }
                            break;
                        }
                        case 0x40: { // RNEXT
                            if (rt != 0) {
                                advance_lfsr(state);
                                uint32_t r_u; std::memcpy(&r_u, &state.r, 4);
                                if (dest_x) state.vf[rt].u[0] = r_u;
                                if (dest_y) state.vf[rt].u[1] = r_u;
                                if (dest_z) state.vf[rt].u[2] = r_u;
                                if (dest_w) state.vf[rt].u[3] = r_u;
                            }
                            break;
                        }
                        case 0x41: { // RGET
                            if (rt != 0) {
                                uint32_t r_u; std::memcpy(&r_u, &state.r, 4);
                                if (dest_x) state.vf[rt].u[0] = r_u;
                                if (dest_y) state.vf[rt].u[1] = r_u;
                                if (dest_z) state.vf[rt].u[2] = r_u;
                                if (dest_w) state.vf[rt].u[3] = r_u;
                            }
                            break;
                        }
                        case 0x42: { // RINIT
                            state.r = ps2_clamp_operand(0x3f800000 | (state.vf[fs].u[Fsf] & 0x007fffff));
                            break;
                        }
                        case 0x43: { // RXOR
                            uint32_t r_u; std::memcpy(&r_u, &state.r, 4);
                            state.r = ps2_clamp_operand(0x3f800000 | ((r_u ^ state.vf[fs].u[Fsf]) & 0x007fffff));
                            break;
                        }
                    }
                } else {
                    // lower1: reuse execute_cop2_instruction by prepending 0x4A000000
                    execute_cop2_instruction(state, 0x4A000000 | (lower & 0x01FFFFFF), nullptr, false);
                }
            } else if ((lower & 0x80000000) != 0) {
                // lower special1: reuse execute_cop2_instruction by prepending 0x4A000000
                execute_cop2_instruction(state, 0x4A000000 | (lower & 0x01FFFFFF), nullptr, false);
            } else {

                // lower2: custom decode
                execute_lower2_instruction(state, lower, pc, new_branch_target, branch_taken_this_cycle);
            }
        }

        if (lower_is_efu) {  // its result lands later
            q_next = state.q;
            state.q = q_before;
            q_pending = lower_special != 0x3b;
            q_ready = issued + (lower_special == 0x3a ? 13 : 7);
            if (lower_special == 0x3b) state.q = q_before;  // WAITQ
        }
        if (hz_swapped >= 0) state.vi[static_cast<size_t>(hz_swapped)] = vi_before[static_cast<size_t>(hz_swapped)];
        if (hz_mask != 0) {  // (this instruction's own write, for the next instruction's branch)
            int wreg = -1;
            if ((upper & 0x80000000) == 0) {
                if ((lower & 0x80000000) != 0) {
                    const uint32_t fn = lower & 0x3f;
                    if ((fn & 0x3c) == 0x3c) {
                        const uint32_t o2 = (lower & 3) | ((lower >> 4) & 0x7c);
                        if (o2 == 0x3c && (hz_mask & 2)) wreg = static_cast<int>((lower >> 16) & 15);
                        else if (o2 == 0x3e && (hz_mask & 4)) wreg = static_cast<int>((lower >> 16) & 15);
                        else if ((o2 == 0x34 || o2 == 0x36) && (hz_mask & 8)) wreg = static_cast<int>((lower >> 11) & 15);
                        else if ((o2 == 0x35 || o2 == 0x37) && (hz_mask & 8)) wreg = static_cast<int>((lower >> 16) & 15);
                    } else if (hz_mask & 1) {
                        if (fn == 0x30 || fn == 0x31 || fn == 0x34 || fn == 0x35) wreg = static_cast<int>((lower >> 6) & 15);
                        else if (fn == 0x32) wreg = static_cast<int>((lower >> 16) & 15);
                    }
                } else {
                    const uint32_t lop = (lower >> 25) & 0x7f;
                    if ((lop == 0x08 || lop == 0x09) && (hz_mask & 1)) wreg = static_cast<int>((lower >> 16) & 15);
                    else if (lop == 0x04 && (hz_mask & 4)) wreg = static_cast<int>((lower >> 16) & 15);
                    else if ((lop == 0x14 || lop == 0x16 || lop == 0x17 || lop == 0x18 || lop == 0x1a || lop == 0x1b || lop == 0x1c) && (hz_mask & 16)) wreg = static_cast<int>((lower >> 16) & 15);
                }
            }
            if (wreg > 0) {
                p_.vi_hz_reg = wreg;
                p_.vi_hz_old = vi_before[static_cast<size_t>(wreg)];
            } else {
                p_.vi_hz_reg = -1;
            }
        }
        // Execute upper instruction (reusing COP2 macro logic) on the registers before the
        // lower's writes; a lane the lower wrote keeps the lower's value.
        // Bit 25-31 are discarded by using 0x01FFFFFF, then we add 0x4A000000 to set COP2 opcode + bit 25 (fmt >= 16)
        const std::array<VUVector, 32> vf_lower = state.vf;
        state.vf = vf_before;
        execute_cop2_instruction(state, 0x4A000000 | (upper & 0x01FFFFFF), nullptr, false);
        for (int r = 1; r < 32; ++r)
            for (int l = 0; l < 4; ++l)
                if (vf_lower[r].u[l] != vf_before[r].u[l]) state.vf[r].u[l] = vf_lower[r].u[l];
        mac_ring[issued & 3] = state.macflag;
        p_.status_ring[issued & 3] = state.statusflag;
        p_.clip_ring[issued & 3] = state.clipflag;
        ++issued;
        if (kick_delay != 0 && --kick_delay == 0) do_kick(kick_addr);

        if (update_i_reg) {
            std::memcpy(&state.i, &new_i_reg, 4);
        }

        if (branch_pending) {
            pc = branch_target;
            branch_pending = false;
        } else {
            pc++;
        }

        if (branch_taken_this_cycle) {
            branch_pending = true;
            branch_target = new_branch_target & p_.pc_mask;
        }

        if (halt_pending) {
            halt_delay--;
            if (halt_delay == 0) {
                halted = true;
            }
        }
        return !halted && pc < p_.size;
}

void VuMicroRun::finish() {
    if (st_ == nullptr) return;
    if (p_.kick_delay != 0) {
        if (st_->xgkick_hook) st_->xgkick_hook(p_.kick_addr);
        else vu_execute_xgkick(*st_, p_.kick_addr, st_->xgkick_primitives);
        p_.kick_delay = 0;
    }
    if (p_.q_pending) {
        st_->q = p_.q_next;
        p_.q_pending = false;
    }
    p_.halted = true;
}

void execute_microprogram(VUState& state, const uint64_t* micromem, size_t micromem_size, uint32_t start_pc,
                          uint64_t max_instructions, VuNativeStep native) {
    VuMicroRun run;
    run.start(state, micromem, micromem_size, start_pc, native);
    for (uint64_t executed = 0; ++executed <= max_instructions && run.step();) {
    }
    run.finish();
}

} // namespace rac1
