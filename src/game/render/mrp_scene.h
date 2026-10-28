// The MRP's per-frame scene state for the mobys (ADR 0010 step 1).
//
// After the game's frame update (the ported moby pass has run: draw loop 0x2559c0, joints 0x24fd80)
// this module has, for every moby the ORIGINAL drew this frame and without running the GS / VU path:
// its class and detail, its world transform, the model-to-view matrix and projection block the VU1
// program is sent, the pose's joint matrices the skinning consumed, the lights and tint the
// skinning lit with, the fade, the special flags, and the list of the parts (VU1 program starts) of
// the moby's chain with every UNPACK they send. Nothing is decided here: every field is read from
// EE memory, from the places the ported code wrote it (each field's source is cited in the struct).
//
// With RAC1_MRP_SCENE_CHECK=1 the faithful path runs alongside and every VU1 moby program start is
// compared with the scene state (mrp_scene_check_*; report with mrp_scene_report()).
#ifndef RAC1_GAME_RENDER_MRP_SCENE_H
#define RAC1_GAME_RENDER_MRP_SCENE_H

#include <cstdint>
#include <vector>

namespace rac1 {

struct VUState;

struct MrpVec4 {
    float x = 0, y = 0, z = 0, w = 0;
};
// Four rows as the VU consumes them (the joint matrices: row 3 the translation).
struct MrpMat4 {
    MrpVec4 r[4];
};

// One VIF UNPACK of a part's packets (what VU1 gets): `num` vectors of format `cmd` from the EE
// address `data` into VU1 memory at quadword `dest` (+ TOPS when `tops`).
struct MrpUnpack {
    uint8_t cmd = 0;  // 0x60 | vn << 2 | vl: 0x6c V4-32, 0x69 V3-16, 0x6e V4-8 ...
    bool tops = false, usn = false;
    uint16_t dest = 0, num = 0;
    uint32_t data = 0;  // EE address of the data
    uint8_t cl = 1, wl = 1;  // the VIF cycle setting in force
    bool is_matrix = false;  // the model-to-view matrix upload (VU1 0x129 / 0x28d, 4 vectors)
};

// One VU1 program start of the moby's chain (the chain's MSCAL) and what it is sent since the
// previous start.
struct MrpPart {
    uint16_t mscal = 0;        // the MSCAL address (0xa / 0xe: 0xe = may need clipping, bit 2 of the draw loop's s3)
    bool has_gs_state = false; // a DIRECT (GS state: the colour fade's ALPHA / FBA / TEST) in the part's packets
    uint32_t direct_data = 0, direct_qwc = 0;  // that DIRECT's GIF packet (EE address, quadwords)
    uint32_t moby_index = 0;   // into MrpFrame::mobys
    // the model-to-view matrix this part's program reads: the latest upload to the part's buffer (VU1 0x129 / 0x28d alternate
    // per part: parts 0 / 1 upload both, later parts reuse the one two parts back); `matrix_dest` its VU1 vector
    MrpMat4 model_to_view;
    uint16_t matrix_dest = 0;
    uint32_t unpack_first = 0, unpack_count = 0;  // into MrpFrame::unpacks
    // a shadow frame's DIRECT read at its capture (its chain is gone afterwards): TEST_1 / ALPHA_1 when set
    bool gs_resolved = false, has_test = false, has_alpha = false;
    uint64_t test = 0, alpha = 0;
};

struct MrpMoby {
    // identity (the moby 0x100 bytes at `moby`; the draw loop's s8)
    uint32_t moby = 0;
    uint32_t class_ptr = 0;  // moby +0x24
    int16_t class_id = 0;    // moby +0xa6
    uint16_t flags = 0;      // moby +0x34 (draw loop t6): 0x1 / 0x80 not drawn; 0x10 extra packets; 0x40 ...; 0x200 alpha-tested; 0xc00 drop shadow
    uint8_t state = 0;       // moby +0x20 (< 0: not drawn)
    // detail selection of the draw loop (0x255fd0): 0 near mesh, 1 far mesh (depth past moby +0x72 << 10)
    bool far_lod = false;
    uint32_t first_packet = 0, packet_count = 0;  // the class's packets (class +0: 0x10 bytes each) drawn: record +0 (count)
    bool deferred = false;    // drawn in the deferred pass (drop shadow seen, 0x256810)
    bool shadow_mesh = false; // the record's +0x14 != 0 (moby +0x73): the flattened shadow mesh (0x256b50)
    bool animated = false;    // record +0x10 low byte != 0 (class +8 / +9)
    // drawn from another view than the frame's (MrpFrame::cam_pos / view_rows: the first moby's): the boot's title main menu
    // draws the scene's mobys from the title's camera, then its boxes from the menu's (0x276128, title_menu_draw 0x2196b8)
    bool other_view = false;
    // world transform (moby +0x10 position, +0xc0 / +0xd0 / +0xe0 the rotation rows, +0x2c scale)
    float pos[3] = {};
    float scale = 0;
    MrpVec4 rot[3];
    // what VU1 is sent: the model-to-view matrix (the chain's UNPACK at VU1 0x129 / 0x28d)
    MrpMat4 model_to_view;
    // lighting, as the skinning's record carries it (draw loop 0x2562f8 / 0x255f?? ; record offsets):
    // directions in model space (record +0x40 / +0x50 / +0x60 column 0, column 1 +0x44.., column 2 the
    // summed point light +0x48..), colours (+0x70 / +0x80 / +0x90), the colour rows' w lanes (+0xa0 / +0xa4)
    MrpVec4 light_dir[3];
    MrpVec4 light_col[3];
    float light_w[2] = {};
    // the colour tint: record +0xb0 (four floats 65536 + byte: moby +0x3c's bytes, the alpha scaled by the fade) as bytes
    uint8_t tint[4] = {};  // r, g, b, alpha (0x80 = opaque)
    uint8_t alpha_base = 0;  // moby +0x23: the alpha before the depth fade (record tint alpha = fade * this >> 7, draw loop 0x255e48)
    // sources in the moby: the light sets (moby +0x38 / +0x39 indices into the level's 0x17fec0 block, +0x3a blend / 4096 << 4)
    uint8_t light_set_a = 0, light_set_b = 0, light_blend = 0;
    // joints: the pose the skinning consumed (0x40 bytes each at the scratchpad's 0), n >= 1
    uint32_t joint_first = 0, joint_count = 0;  // into MrpFrame::joints
    // the chain
    uint32_t chain_begin = 0, chain_end = 0;  // the moby's parts in the packet buffer
    uint32_t ring = 0;                        // the record's place in the ring (0x15f5f8 on): the skinning's input
    uint32_t record_first = 0;                // into MrpFrame::records (bytes, 0x10 * record_qwords)
    uint32_t record_qwords = 0;
    uint32_t part_first = 0, part_count = 0;  // into MrpFrame::parts
    // the parts given by the draw loop (the MRP-only moby pass, moby_pass.h g_moby_pass_mrp), not decoded from the chain (which
    // has none): no unpacks, their GS state read at once (MrpPart::gs_resolved)
    bool direct = false;
    // (testing, RAC1_MOBY_PASS_MRP=verify: the parts the draw loop gave, in MrpFrame::check_parts, against the decoded ones)
    uint32_t check_first = 0, check_count = 0;
    MrpMat4 check_model_to_view;
    // drawn into a render target: the game's view was a texture's when the draw loop ran (render_target_view 0x1f7fa0 set
    // the screen globals 0x13e500 / 0x13e504 to the texture's size, not the display's 0x1518d0 / 0x1518d2): the menus' item models
    // (0x24ff08 called by the widgets' draws between 0x1fc928 and 0x1fca18). Then the projection block the loop sent at this call
    // (0x16ca40 x 6, copied into the chain's head by 0x2559c0) and the target's size; MrpFrame::proj stays the screen's.
    bool rt = false;
    MrpVec4 rt_proj[6];
    int32_t rt_w = 0, rt_h = 0;
};

struct MrpFrame {
    // camera / projection: the VU1 constant block 0x16ca40..0x16ca9f (6 vectors), as the draw loop sends it
    MrpVec4 proj[6];
    // the draw loop's view: camera position 0x166dc0 (x 1024 in the loop), the world-to-view rows 0x166cc0 / d0 / e0 and 0x166cf0
    MrpVec4 cam_pos;
    MrpVec4 view_rows[3];
    MrpVec4 view_trans;
    std::vector<MrpMoby> mobys;  // in the order of the chain (= of the VU1 starts)
    std::vector<MrpPart> parts;
    std::vector<MrpUnpack> unpacks;
    std::vector<MrpMat4> joints;
    std::vector<uint8_t> records;
    std::vector<MrpPart> check_parts;  // (testing: RAC1_MOBY_PASS_MRP=verify, MrpMoby::check_first)
};

// orig: (host) hooks the observers of the moby pass (moby_draw.h / moby_skin.h)
void mrp_scene_install();
// orig: (host) before the game's frame update: clears the scene
void mrp_scene_begin_frame();
// orig: (host) after the game's frame update: decodes the chains of the mobys drawn
void mrp_scene_end_frame();
// orig: (host) the frame's scene (valid until the next begin_frame)
const MrpFrame& mrp_scene();
// orig: (host) the scene the MRP draws: the moby pass's shadow run's (mrp_lod.h: the LOD slider / the widened view)
// when this update made one, else mrp_scene(). (mrp_scene() stays the real pass's: the GS replica's skips follow its chain.)
const MrpFrame& mrp_scene_draw();
// orig: (host) around the moby pass's shadow run (inside its memory journal): begin keeps the frame observed so
// far aside and starts an empty one; end decodes the shadow's chains while their memory is there (the parts' DIRECT state
// read at once), keeps it as the draw scene and puts the kept frame back
void mrp_scene_shadow_begin();
void mrp_scene_shadow_end();
// orig: (host) the totals of the check that every moby the real pass drew is in the scene the MRP draws (RAC1_MRP_LOD_DROPS=1 logs each)
void mrp_scene_lod_drop_report();

// Differential check against the faithful VU1 path (RAC1_MRP_SCENE_CHECK=1).
bool mrp_scene_check_enabled();
// orig: (host) the faithful chain is about to run (`runs`; `previous_frames_chain`: it is the chain of the frame before, kicked by the
// level exit's loops, not the scene's): the start counter to 0
void mrp_scene_check_begin_chain(bool runs, bool previous_frames_chain);
// orig: (host) a VU1 moby program starts (the moby program's hash matched) at `start`: compares the VU1 memory with the scene
void mrp_scene_check_start(const VUState& st, uint32_t start);
// orig: (host) the faithful chain ran: the starts counted against the scene's parts
void mrp_scene_check_end_chain();
// orig: (host) prints the totals and every mismatch class
void mrp_scene_report();

}  // namespace rac1

#endif
