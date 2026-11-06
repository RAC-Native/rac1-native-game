// The world culls written by hand (world_cull_tfrag.cpp, world_cull_tie.cpp, world_cull_shrub.cpp): what they share. The scratchpad
// as a byte view (the culls keep their records, lists and packet buffers there: one host buffer, which a journal saves whole), the
// scratchpad DMA kicks through the DMA registers (eer::hw_sw: the transfer at its kick, as the transliteration does), the switch
// between the hand-written culls and the transliterated ones, and its check mode.
// orig: (host)
#ifndef RAC1_GAME_OVL_WORLD_CULL_HAND_H
#define RAC1_GAME_OVL_WORLD_CULL_HAND_H

#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/vu0_local.h>
#include <runtime/kernel/ee_memory.h>

#include <cstdint>
#include <cstring>
#include <functional>

namespace rac1::world_hand {

// orig: (host) a quadword as two doublewords (the templates the culls copy into their packets)
struct Qw {
    uint64_t lo, hi;
};

// orig: (host) the scratchpad (0x70000000, 16 KB) by EE address: words native, bytes and halfwords at their EE byte offset
// (identical on a little-endian host; a big-endian host keeps EE memory as host-order words)
struct Spr {
    uint8_t* const base = ee_detail::g_spr;

    static constexpr uint32_t off(uint32_t a) { return a & 0x3fffu; }
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    static constexpr uint32_t boff(uint32_t a) { return off(a) ^ 3u; }
    static constexpr uint32_t hoff(uint32_t a) { return off(a) ^ 2u; }
#else
    static constexpr uint32_t boff(uint32_t a) { return off(a); }
    static constexpr uint32_t hoff(uint32_t a) { return off(a); }
#endif
    uint8_t u8(uint32_t a) const { return base[boff(a)]; }
    uint16_t u16(uint32_t a) const {
        uint16_t v;
        std::memcpy(&v, base + hoff(a), 2);
        return v;
    }
    uint32_t u32(uint32_t a) const {
        uint32_t v;
        std::memcpy(&v, base + off(a), 4);
        return v;
    }
    Qw q(uint32_t a) const {
        Qw v{};
        std::memcpy(&v, base + (off(a) & ~15u), 16);
        return v;
    }
    void set8(uint32_t a, uint32_t v) const { base[boff(a)] = static_cast<uint8_t>(v); }
    void set16(uint32_t a, uint32_t v) const {
        const uint16_t h = static_cast<uint16_t>(v);
        std::memcpy(base + hoff(a), &h, 2);
    }
    void set32(uint32_t a, uint32_t v) const { std::memcpy(base + off(a), &v, 4); }
    void set64(uint32_t a, uint64_t v) const { std::memcpy(base + (off(a) & ~7u), &v, 8); }
    void setq(uint32_t a, const Qw& v) const { std::memcpy(base + (off(a) & ~15u), &v, 16); }
    // a VF register's four lanes (LQC2 / SQC2)
    template <class V>
    void load_vf(V& u, int r, uint32_t a) const {
        const Qw v = q(a);
        u.set_words(r, static_cast<uint32_t>(v.lo), static_cast<uint32_t>(v.lo >> 32), static_cast<uint32_t>(v.hi), static_cast<uint32_t>(v.hi >> 32));
    }
    template <class V>
    void store_vf(const V& u, int r, uint32_t a) const { std::memcpy(base + (off(a) & ~15u), &u.vf[r], 16); }
};

// orig: (host) a quadword of EE RAM (LQ / SQ) and a VF register from / to RAM (LQC2 / SQC2)
inline Qw ram_q(uint32_t a) { return ee_read<Qw>(a & ~15u); }
inline void ram_setq(uint32_t a, const Qw& v) { ee_write<Qw>(a & ~15u, v); }
template <class V>
inline void ram_load_vf(V& u, int r, uint32_t a) {
    const Qw v = ram_q(a);
    u.set_words(r, static_cast<uint32_t>(v.lo), static_cast<uint32_t>(v.lo >> 32), static_cast<uint32_t>(v.hi), static_cast<uint32_t>(v.hi >> 32));
}
template <class V>
inline void ram_store_vf(const V& u, int r, uint32_t a) {
    Qw v{};
    std::memcpy(&v, &u.vf[r], 16);
    ram_setq(a, v);
}

// orig: (helper) the scratchpad DMA channels' kicks as the culls write them: the address, count and scratchpad address
// registers, then CHCR with STR (the transfer runs at the kick, eer::hw_sw). toSPR (channel 9, 0x1000d400): `qwc`
// quadwords from RAM `madr` to the scratchpad's `sadr`; fromSPR (channel 8, 0x1000d000): back to RAM. (The culls wait for a
// channel before each kick: here it never runs on, its STR is clear.)
inline void dma_to_spr(uint32_t sadr, uint32_t madr, uint32_t qwc) {
    eer::hw_sw(0x1000d480u, sadr);
    eer::hw_sw(0x1000d410u, madr);
    eer::hw_sw(0x1000d420u, qwc);
    eer::hw_sw(0x1000d400u, 0x100);
}
inline void dma_from_spr(uint32_t madr, uint32_t sadr, uint32_t qwc) {
    eer::hw_sw(0x1000d080u, sadr);
    eer::hw_sw(0x1000d010u, madr);
    eer::hw_sw(0x1000d020u, qwc);
    eer::hw_sw(0x1000d000u, 0x100);
}
// orig: (helper) the culls' packet buffer flush: the part of the scratchpad buffer at `sp` filled so far (`used` = sp & mask)
// out to the frame packet at `pkt` - used, the other half of the double buffer taken (sp ^ `half`). Returns the new sp.
inline uint32_t flush_packets(uint32_t sp, uint32_t pkt, uint32_t used, uint32_t half) {
    dma_from_spr(pkt - used, sp - used, used >> 4);
    return (sp - used) ^ half;
}

// orig: (host) the rewritten culls and lighting (true: done; false: not usable now, the caller runs the transliteration)
bool tfrag_cull();
bool tie_cull();
bool shrub_cull();
bool tfrag_lighting(uint32_t list);

// orig: (host) a hand-written routine checked against its transliteration (a switch's mode 2, world_cull_fast.cpp): the
// hand-written one runs inside a journal of EE memory, its results (the EE RAM, the scratchpad, the VU0 registers and flags, the
// DMA registers, the world passes' visible lists) are kept, the memory put back, then the transliteration runs and its results
// are compared; the game goes on with the transliteration's. Differences are printed (the first few) and counted; the counts are
// printed at the end of the run (world_cull_check_report). Slow: two 32 MB copies a call; RAC1_HAND_CHECK_EVERY=n checks about one
// call in n, chosen at random (the others run the transliteration alone), for the routines called many times a frame.
struct HandCheck {
    const char* tag;   // the messages' prefix
    const char* var;   // the switch
    const char* name;  // the routine
    unsigned long long calls = 0, differing = 0;  // (the calls checked)
};
void hand_check(HandCheck& c, const std::function<bool()>& hand, const std::function<void()>& tr);

}  // namespace rac1::world_hand

#endif
