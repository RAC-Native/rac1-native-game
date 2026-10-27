// The GS's 4 MB local memory as the game sees it: the bytes its uploads write (libgraph's image loads, the IMAGE transfers in
// the frame's chain) with the GS's swizzled page / block / column layouts, and what reads them: libgraph's GS -> EE transfers
// (the game stores parts of it back into EE memory: the pause menu, movie_mode_end 0x2997c8 - so these bytes are game state)
// and the texture decodes of the drawers. Nothing is drawn into it (the host draws on the GPU; the frame's picture is put there
// by the host when the game stores it, StoreImageHook below).
//
// Shared by the GS replica's walk (gs_emu.h, the faithful reference) and the MRP's chain reader (src/game/render/mrp_chain.h):
// one of them walks a frame's chain into it, never both.
#ifndef RAC1_RUNTIME_GS_GS_MEMORY_H
#define RAC1_RUNTIME_GS_GS_MEMORY_H

#include <runtime/gs/gs_types.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace rac1::gs {

class LocalMemory {
public:
    LocalMemory();
    uint8_t* vram() { return vram_.data(); }
    const uint8_t* vram() const { return vram_.data(); }
    uint64_t generation() const { return gen_; }  // bumped by every upload that changes the memory

    // Addressing (bytes; PSMT4: half bytes) of pixel (x, y) in a buffer at block `bp` of width `bw` (in 64 pixels).
    static uint32_t addr32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
    static uint32_t addr16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, bool s);
    static uint32_t addr8(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
    static uint32_t addr4(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);  // half bytes
    uint32_t read_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) const;
    void write_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v);
    void write_pixel_raw(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v);
    // (host) a w x h CT32 picture written at block bp (bw x 64 wide) at once: the memory's change noted once (a store
    // hook's frame: write_pixel's per-pixel bookkeeping cost ~150 ms for a 512 x 416 frame)
    void write_ct32_rect(uint32_t bp, uint32_t bw, uint32_t w, uint32_t h, const uint32_t* px);

    // A texel of the texture TEX0 `tex0` (CLUT applied, TEXA for 16/24 bit): RGBA, alpha raw (0x80 = 1).
    uint32_t texel(uint64_t tex0, uint64_t texa, int x, int y) const;
    // The texture TEX0 / TEXA as the memory holds it now, decoded once: the same image while the bytes it read (its pixels and
    // its CLUT) are unchanged (an upload that writes the bytes already there changes nothing, so a texture the game loads
    // again each frame is decoded once).
    std::shared_ptr<const TexImage> texture_at_kick(uint64_t tex0, uint64_t texa);
    // (host) the w x h CT32 picture at block bp (bw x 64 wide), as one image, decoded again only when those bytes changed
    // (the frame an IMAGE transfer put back: gs_emu.h's type-7 primitive)
    std::shared_ptr<const TexImage> picture_ct32(uint32_t bp, uint32_t bw, uint32_t w, uint32_t h);

    // The host -> local transfer (TRXDIR 0 starts it; BITBLTBUF / TRXPOS / TRXREG as the GS registers hold them at each piece of
    // its data, as the GS reads them): `p` the next `n` bytes of its pixels.
    void transfer_start();
    void transfer_stop() { xfer_ = false; }
    bool transfer_active() const { return xfer_; }
    void transfer_data(uint64_t bitbltbuf, uint64_t trxpos, uint64_t trxreg, const uint8_t* p, uint32_t n);
    struct TransferState {
        bool on = false;
        uint32_t xx = 0, xy = 0;
        std::vector<uint8_t> pend;
    };
    TransferState transfer_state() const { return TransferState{xfer_, xx_, xy_, xpend_}; }
    void set_transfer_state(const TransferState& t);

    // (host) the writes from undo_begin() on kept, put back by undo_restore() (a widened view's substitute chain walked before
    // the real range it stands for: its uploads must not reach the real range's textures)
    void undo_begin();
    void undo_restore();
    void undo_end() { vundo_on_ = false; }  // (the writes no longer kept, those kept not put back)

    // (host) the decoded textures forgotten (a level change: the GS worker and the drawers idle). Their count before.
    size_t clear_caches();
    size_t cached_textures() const { return tex_snaps_.size(); }

private:
    void note_change(uint32_t a, uint32_t n);
    uint32_t store(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v, uint32_t& n_out);
    std::vector<uint8_t> vram_;
    uint64_t gen_ = 0;
    // the host -> local transfer in progress
    bool xfer_ = false;
    uint32_t xx_ = 0, xy_ = 0;
    std::vector<uint8_t> xpend_;  // partial pixels between packets (24 / 4 bit)
    struct VUndo {
        uint32_t a, n, before;
    };
    std::vector<VUndo> vundo_;
    bool vundo_on_ = false;
    // the textures read since the memory last changed (TEX0 without CLD, TEXA) -> image
    // (valid while the local memory it read, [rd_lo, rd_hi), has not changed since `seq`)
    struct TexSnap {
        uint64_t gen = ~0ull;
        std::shared_ptr<const TexImage> img;
        uint64_t seq = 0;
        uint32_t rd_lo[2] = {0, 0}, rd_hi[2] = {0, 0};  // the texture's bytes, and its CLUT's
    };
    bool snap_stale(const TexSnap& s) const;
    bool changed_since(uint64_t seq, uint32_t lo, uint32_t hi) const;
    std::map<std::pair<uint64_t, uint64_t>, TexSnap> tex_snaps_;
    uint64_t tex_ids_ = 0;
    // the writes that changed the local memory's bytes: [lo, hi) per run of changes, numbered
    struct WrRange {
        uint64_t seq;
        uint32_t lo, hi;
    };
    std::vector<WrRange> wr_log_;
    uint64_t wr_seq_ = 0;    // the last number given
    uint64_t wr_floor_ = 0;  // the log is complete after this number
    bool wr_open_ = false;   // the last entry still takes changes
    mutable bool rd_track_ = false;  // a texture decode notes the bytes it reads
    mutable uint32_t rd_lo_[2] = {0xffffffffu, 0xffffffffu}, rd_hi_[2] = {0, 0};
    mutable int rd_slot_ = 0;  // 0: the texture's pixels, 1: its CLUT
    // picture_ct32's last picture
    std::shared_ptr<const TexImage> pic_;
    uint64_t pic_seq_ = 0;
    uint32_t pic_bp_ = ~0u, pic_bw_ = 0, pic_w_ = 0, pic_h_ = 0;
};

// The one local memory the game's uploads go to.
LocalMemory& local_memory();

// (host) Before anything outside a chain's walk uses the local memory (libgraph's transfers, the drawing, a level change), the
// walk running on a worker thread is waited for: every walker registers how to wait for it.
using MemoryJoin = void (*)();
void add_memory_join(MemoryJoin join);
void memory_join();

// Host: called by the game's GS -> EE transfers (libgraph's 0x1227d8) before they read the local memory at block `bp`
// (`bw` x 64 pixels wide, format `psm`): the host may put there what the GS would hold - the frame it drew, which nothing draws into
// this memory. nullptr: none.
using StoreImageHook = void (*)(uint32_t bp, uint32_t bw, uint32_t psm);
void set_store_image_hook(StoreImageHook hook);
StoreImageHook store_image_hook();

}  // namespace rac1::gs

#endif
