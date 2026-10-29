// The Modern Rendering Pipeline's 3D effects (ADR 0010): the particle pass's (0x267c98) GIF output, and
// the EE-built effects (the quad drawer's glows / canopy / decals / effect quads, the sparkles, the nanotech orbs' prim lists),
// drawn natively, in the world's projection, batched. Host code: none of it is the game's.
//
// The particle pass builds a VU1 chain (the particle program, its constants, 0x800-byte chunks of particle records); the MRP's chain
// reader (mrp_chain.h) runs that range - the ported particle program (particles_vu1.cpp) on its own VU1, its kicks read as GIF
// packets, each primitive's texture decoded from the GS's local memory at its kick - and keeps the primitives (gs::Primitive::fx,
// MrpChainInput::fx). The MRP then draws them:
//  - each kicked vertex (X, Y, Z as the program computed them from the particle's camera-relative position) taken back to the world
//    through the program's own projection (the view rows 0x166cc0 x 1024, the screen offset 0x16cbe0, Q = 0x16cc50.x / w) and drawn
//    in the world's GL projection (the same depth buffer as the world and the mobys; in a widened view the side bands get it);
//  - its texture resident (decoded once, an atlas of 256 x 256 layers), the GS state (texture function, alpha test, blending, the
//    GS's fog, the Z test / mask) in the fragment shader / GL state;
//  - all of the frame's particles in a handful of draws (a batch breaks only on the GL blend / depth state);
//  - in the chain's primitive order, the 2D between them (mrp_2d.h; MrpChainFrame::fx_mark).
// In a widened view the particles come from the effects' shadow run (its widened sphere test; the streaks' frustum test widened
// too, mrp_fx_particles_patch), the chain reader's substitute (MrpChainInput::subs).
#ifndef RAC1_GAME_RENDER_MRP_FX_H
#define RAC1_GAME_RENDER_MRP_FX_H

#include <game/render/mrp_chain.h>
#include <runtime/gs/gs_types.h>
#include <runtime/vu/vu_interpreter.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace rac1 {

// The switch (RAC1_MRP_FX=0/1, the debug menu's Rendering tab): the 3D effects drawn by the MRP (default on with the MRP world). Off:
// the GS replica reads the frame's chain and draws them (the faithful path, mrp_chain_wanted). RAC1_MRP_FX_KINDS=<mask> (testing): only the producers whose bit
// (1 << kind) is set (2 = the particles alone).
extern bool g_mrp_fx;

// The producers of 3D effects whose primitives the MRP draws, each with its own projection from the camera-relative
// point to the GS's (X, Y, Z):
//  - kMrpFxParticles: the particle program (0x267c98's VU1 program; the sky's sprites are the MRP sky's): the view rows 0x166cc0
//    (x 1024 but the last), the screen offset 0x16cbe0, Q = 0x16cc50.x / w;
//  - kMrpFxQuad: the quad drawer 0x1fd228 (the glows 0x263618 / 0x2df3d8 / 0x207d40, the ships' canopy 0x2935b8, the decals 0x1f9660,
//    the effect quads of the draw lists' callbacks): the same rows on the point x 0x160be0.z, Q = 0x160bf0.x / w, the offset 0x16cbe0;
//    its clipper's vertices (0x1fd89c: the guard band, in the clip space of the same rows) are on the quad's plane;
//  - kMrpFxSparkles: the sparkles 0x1fead8: the rows x 1024, Q = 0x16cc50.x / w, X / Y + 2048, Z x 0.99969 + 0x16cbe8; the four
//    corners offset on the screen at the centre's Z;
//  - kMrpFxPrimList: the prim lists 0x1fde98 (the nanotech orbs' mesh) on VU1 program 7: the matrix the last 0x1fc740 put in the
//    chain (0x166cc0 x the world-to-camera x 1024, its Z bias), its screen offset and Q numerator.
//  - kMrpFxVu7: VU1 program 7's batches inside the draw lists' callbacks (the levels' water, lava and sand-sea surfaces, the prim
//    lists above): not their kicks - the chain reader keeps each batch as the program found it, before its transform and its clipping
//    at the guard band (MrpChainInput::vu7) - but their input points, drawn in the world (g_mrp_seas).
enum : uint8_t { kMrpFxParticles = 1, kMrpFxQuad = 2, kMrpFxSparkles = 3, kMrpFxPrimList = 4, kMrpFxVu7 = 5, kMrpFxKinds = 6 };
// The switch (RAC1_MRP_SEAS=0/1, default on): VU1 program 7's surfaces drawn by the MRP from their input (kMrpFxVu7). Off: as before
// (the GS replica reads the chain and draws them, the faithful path)
extern bool g_mrp_seas;
// orig: (host) the hash of VU1 program 7's code (shadows_vu1.cpp's registration)
constexpr uint64_t kMrpVu7Hash = 0x3dc739c28ad6ece5ull;

// orig: (host) at render_frame's start: the last update's notes forgotten
void mrp_fx_frame_begin();
// orig: (host) render_frame's effects code runs (`on`: the draw lists before the mobys, the texture block after them up to the
// screen effects): the producers' notes are taken only then (the moby pass's shadow run, the oracle: none)
void mrp_fx_window(bool on);
// orig: (host) a producer's chain range [first, end) of the update running now (of the real block or of the effects shadow, mrp_lod.h)
void mrp_fx_note(uint8_t kind, uint32_t first, uint32_t end);
inline void mrp_fx_note_particles(uint32_t first, uint32_t end) { mrp_fx_note(kMrpFxParticles, first, end); }
// orig: (host) 0x1fc740 (shadows_vu1_setup) wrote VU1 program 7's constants at `qw` (EE address of the UNPACK's data: the two matrices,
// the fans' tag, vf29, vf30): the prim lists that follow take their projection from there
void mrp_fx_note_vu7(uint32_t qw);
// orig: (host) a producer's range noted from its start to its end (the packet cursor 0x1611c0 then and at the scope's end). `a` / `b`:
// for the vertex check (RAC1_MRP_FX_VERIFY) the producer's input - kMrpFxQuad the descriptor and the matrix (0x1fd228's a0 / a1),
// kMrpFxPrimList the positions and the count (0x1fde98's xyz / n), kMrpFxSparkles none (the slots 0x16ea40)
class MrpFxScope {
public:
    explicit MrpFxScope(uint8_t kind, uint32_t a = 0, uint32_t b = 0);
    ~MrpFxScope();
    MrpFxScope(const MrpFxScope&) = delete;
    MrpFxScope& operator=(const MrpFxScope&) = delete;

private:
    uint8_t kind_;
    uint32_t first_;
    bool on_;
    uint32_t pending_vin_ = 0, pending_vn_ = 0;  // (the vertex check's input)
};
// orig: (host) a draw list's callback wrote [first, end) of the chain in the update running now (the real block or a shadow's): its
// VU1 program 7 batches are the MRP's (kMrpFxVu7); `cls` the moby class of its argument (0: not a moby), the emissive surfaces' key
void mrp_fx_note_list(uint32_t first, uint32_t end, uint32_t cls);
// orig: (host) testing (RAC1_MRP_FX_INVENTORY=1): a draw list's callback `fn` wrote [first, end) of the chain; what no producer's note
// covers is counted per callback (the report: the effects no producer notes)
void mrp_fx_list_call(uint32_t fn, uint32_t first, uint32_t end);
// orig: (host) around the particle pass inside the effects shadow (`begin` true before it, false after): the streaks' frustum test
// widened as the sphere test is (the journal puts it back)
void mrp_fx_particles_patch(bool begin);
// orig: (host) whether the particle pass 0x267c98 could draw a particle with its frustum tests widened by `widen` (the
// effects shadow's patch_cull and mrp_fx_particles_patch) that it does not draw with the game's own: its sphere test (sprites, lines)
// and streak test (both ends through the clip rows 0x166d40) made in double on the pool, with a margin either way (conservative: true
// whenever a particle is near the 4:3 frustum's sides and inside the widened one; the depth range left out, which only says true more)
bool mrp_fx_particles_widen_needed(float widen);

// orig: (host) per host frame, in the chain's stage (the game's thread, after the update): the effects ranges for the chain about to be
// read (`subs`: the shadow blocks whose chains are read in place of the real blocks, mrp_lod_shadow_slot's slots, in the order given
// to MrpChainInput::subs) and the projections the producers used, kept for the drawing. `active`: a frame whose world the MRP draws.
// Returns false when no effect of this frame is the MRP's.
// `vu7`: the draw lists' ranges whose VU1 program 7 batches the MRP draws (MrpChainInput::vu7), in the same order.
bool mrp_fx_chain_ranges(bool active, const std::vector<int>& subs, std::vector<MrpChainInput::Fx>& ranges, std::vector<MrpChainInput::Vu7>& vu7);
// orig: (host) the frame's primitives drawn in their order (the chain's reading joined): the runs of the MRP's (fx) by the MRP, the
// runs between them by `gs_draw(from, to)` (the 2D: mrp_2d_draw over those primitives); into the bound framebuffer, viewport `vp`
// (`from` / `to`: only the primitives [from, to): the ones before the moby pass go under the MRP's mobys, MrpChainFrame::moby_mark)
// `skip_surfaces`: program 7's surfaces that write Z left out (mrp_fx_draw_surfaces drew them)
void mrp_fx_draw(const MrpChainFrame& gs, int win_w, int win_h, const gs::Viewport* vp, const std::function<void(size_t, size_t)>& gs_draw, size_t from = 0,
                 size_t to = SIZE_MAX, bool skip_surfaces = false);
// orig: (host) program 7's surfaces that write Z (the levels' seas, lava and sand) among the primitives from `from`, drawn alone: before
// the ambient particles, which write no Z (drawn after them, the seas covered the rain, the snow and the embers in front of them);
// mrp_fx_draw then leaves them out (skip_surfaces). The translucent effects and the 2D keep their place in the chain's order.
void mrp_fx_draw_surfaces(const MrpChainFrame& f, int win_w, int win_h, const gs::Viewport* vp, size_t from);
// orig: (host) testing (RAC1_MRP_FX_KICKCHECK=1): a start of the particle program (`hash`) on the chain reader's VU1 (`vu`: a vu1::Vu1)
// run on the VU interpreter from a copy of the same state as well, its kicks and data memory compared with the port's; true when it ran
// the program (the port, its kicks to `kick`)
bool mrp_fx_vu1_check(void* vu, uint32_t start, uint64_t hash, void (*fn)(VUState&, uint32_t, const std::function<void(uint32_t)>&),
                      const std::function<void(uint32_t)>& kick);
// orig: (host) the effects' texture atlas emptied (a level change, between frames): its layers' count before
size_t mrp_fx_clear_caches();
// orig: (host) the totals at the run's end
void mrp_fx_report();

struct MrpFxWiiuBatch {
    uint64_t prim = 0, test = 0, alpha = 0, zbuf = 0;
    uint32_t fogcol = 0, pass2 = 0, sampler = 0, ztst = 2;
    bool ztest = false, zwrite = false, ate = false, fge = false;
    std::shared_ptr<const gs::TexImage> tex;
    uint32_t first = 0, count = 0;
};

struct MrpFxWiiuView {
    uint64_t serial = 0;
    const void* verts = nullptr;  // const mrpwb::GpuV*
    size_t nverts = 0;
    const std::vector<MrpFxWiiuBatch>* batches = nullptr;
};

// orig: (host) builds the Wii U GX2 vertex/batch snapshot from `gs` (called after mrp_chain_join on Wii U)
void mrp_fx_wiiu_prepare(const MrpChainFrame& gs, const float* vpm_opt = nullptr);
// orig: (host) reads the latest prepared Wii U GX2 effects snapshot
bool mrp_fx_wiiu_view(MrpFxWiiuView& out);

}  // namespace rac1

#endif
