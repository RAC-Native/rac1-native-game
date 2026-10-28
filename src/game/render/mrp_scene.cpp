// orig: (host) The MRP's per-frame scene state for the mobys (see mrp_scene.h).
//
// Sources (all EE memory written by the ported moby pass):
//  - the draw loop's observer (moby_draw.h) gives, per moby drawn, the moby, its chain parts and its
//    skinning record (the 0x10-byte units the loop built on the scratchpad, then sent to the ring);
//  - the pose observer (moby_skin.h) gives the joint matrices on the scratchpad the skinning reads;
//  - the chain (DMA tags + VIF codes in the packet buffer) is decoded into the parts and UNPACKs.
#include <game/render/mrp_scene.h>

#include <game/ovl/moby_draw.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/moby_skin.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/vu/vu_interpreter.h>
#include <jobs/job_pool.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace rac1 {

namespace {

constexpr uint32_t SPR = 0x70000000u;

MrpFrame g_frame;
MrpFrame g_shadow_frame, g_kept_frame;
bool g_shadow_valid = false;
size_t g_next_pose = 0;
size_t g_last_pose = ~size_t(0);  // the moby of the pose just seen (on_pose), for the skinning's skip
bool g_have_camera = false;
int g_frame_no = 0;

const bool g_check = std::getenv("RAC1_MRP_SCENE_CHECK") != nullptr;
const bool g_prof = std::getenv("RAC1_FRAME_PROF") != nullptr;
double g_prof_obs_ms = 0, g_prof_decode_ms = 0;
int g_prof_frames = 0;

double now_ms() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// orig: (host) a vector at an EE address
MrpVec4 vec4_at(uint32_t a) { return MrpVec4{lwc1(a), lwc1(a + 4), lwc1(a + 8), lwc1(a + 12)}; }
MrpMat4 mat4_at(uint32_t a) {
    MrpMat4 m;
    for (int i = 0; i < 4; ++i) m.r[i] = vec4_at(a + 16 * static_cast<uint32_t>(i));
    return m;
}

// orig: (host) the GS state of a part's DIRECT (A+D, PACKED: the frame's TEST_1 0x47 / ALPHA_1 0x42) from its GIF packet, `qwc`
// quadwords read by `q(i)` (the quadword's four words): as mrp_moby_renderer.cpp's gif_ad reads it at the draw
template <typename Q>
void gs_resolve(MrpPart& p, uint32_t qwc, Q q) {
    p.gs_resolved = true;
    uint32_t off = 0;
    while (off < qwc) {
        uint32_t w[4];
        q(off, w);
        const uint64_t lo = static_cast<uint64_t>(w[0]) | (static_cast<uint64_t>(w[1]) << 32);
        const uint64_t hi = static_cast<uint64_t>(w[2]) | (static_cast<uint64_t>(w[3]) << 32);
        const uint32_t nloop = lo & 0x7fff, flg = (lo >> 58) & 3, nreg0 = (lo >> 60) & 0xf;
        const uint32_t nreg = nreg0 == 0 ? 16 : nreg0;
        if (flg != 0) break;
        for (uint32_t i = 0; i < nloop; ++i)
            for (uint32_t r = 0; r < nreg; ++r) {
                if (((hi >> (4 * r)) & 0xf) != 0xe) continue;
                uint32_t d[4];
                q(off + 1 + i * nreg + r, d);
                const uint64_t v = static_cast<uint64_t>(d[0]) | (static_cast<uint64_t>(d[1]) << 32);
                const uint32_t reg = d[2] & 0xff;
                if (reg == 0x47) p.test = v, p.has_test = true;
                if (reg == 0x42) p.alpha = v, p.has_alpha = true;
            }
        off += 1 + nloop * nreg;
        if ((lo >> 15) & 1) break;
    }
}
// orig: (host) the same from EE memory at `a`
void gs_resolve_at(MrpPart& p, uint32_t a, uint32_t qwc) {
    gs_resolve(p, qwc, [a](uint32_t i, uint32_t w[4]) {
        for (uint32_t k = 0; k < 4; ++k) w[k] = lwu(a + i * 16 + 4 * k);
    });
}

// orig: (host) the parts the MRP-only draw loop gave (moby_draw.h MobyDrawPart) as decode_chain_to makes them from the chain:
// each MSCAL a part, its model-to-view matrix the latest upload to its buffer (its own, or the one two parts back's), the
// moby's the first upload; appended to `parts`
void direct_parts(uint32_t mi, const MobyDrawPart* dp, uint32_t n, std::vector<MrpPart>& parts, uint32_t unpack_first, uint32_t& first,
                  MrpMat4& model_to_view) {
    first = static_cast<uint32_t>(parts.size());
    bool have_matrix = false;
    MrpMat4 last_mat[2];
    bool have_last[2] = {false, false};
    for (uint32_t k = 0; k < n; ++k) {
        const MobyDrawPart& d = dp[k];
        MrpPart cur;
        cur.moby_index = mi;
        cur.unpack_first = unpack_first;
        cur.mscal = d.mscal;
        if (d.matrix_dest != 0) {
            MrpMat4 mm;
            std::memcpy(&mm, d.matrix, sizeof mm);
            if (!have_matrix) {
                model_to_view = mm;
                have_matrix = true;
            }
            const int bi = d.matrix_dest == 0x28d ? 1 : 0;
            last_mat[bi] = mm;
            have_last[bi] = true;
            cur.matrix_dest = d.matrix_dest;
        } else if (k >= 2) {
            cur.matrix_dest = parts[parts.size() - 2].matrix_dest;
        } else if (k == 1) {
            cur.matrix_dest = parts.back().matrix_dest == 0x129 ? 0x28d : 0x129;
        } else {
            cur.matrix_dest = 0x129;
        }
        const int bi = cur.matrix_dest == 0x28d ? 1 : 0;
        if (have_last[bi]) cur.model_to_view = last_mat[bi];
        if (d.gs_qwc != 0) {
            cur.has_gs_state = true;
            cur.direct_qwc = d.gs_qwc;
        }
        gs_resolve(cur, d.gs_qwc, [&d](uint32_t i, uint32_t w[4]) {
            for (uint32_t j = 0; j < 4; ++j) w[j] = i < 2 ? d.gs[4 * i + j] : 0u;
        });
        parts.push_back(cur);
    }
}

// ---- observers -----------------------------------------------------------------------------

// orig: (host) the draw loop's observer (0x002559c0, at the record's send, 0x256668)
void on_draw(uint32_t moby, uint32_t chain_begin, uint32_t chain_end, uint32_t ring, uint32_t spr_rec, uint32_t qwords, uint32_t far_lod, uint32_t deferred,
             const MobyDrawPart* parts, uint32_t nparts) {
    const double t0 = g_prof ? now_ms() : 0.0;
    MrpFrame& f = g_frame;
    // a moby drawn into a render target (the screen globals the texture's size, not the display's)
    const int32_t view_w = lw(0x0013e500), view_h = lw(0x0013e504);
    const bool rt = view_w != static_cast<int32_t>(lhu(0x001518d0)) || view_h != static_cast<int32_t>(lhu(0x001518d2));
    if (!g_have_camera && !rt) {  // the VU1 constant block and the view the loop sends / uses (0x16ca40 x 6, 0x166dc0, 0x166cc0..0x166cf0)
        for (uint32_t i = 0; i < 6; ++i) f.proj[i] = vec4_at(OV(0x0016ca40) + 16 * i);
        f.cam_pos = vec4_at(OV(0x00166dc0));
        for (uint32_t i = 0; i < 3; ++i) f.view_rows[i] = vec4_at(OV(0x00166cc0) + 16 * i);
        f.view_trans = vec4_at(OV(0x00166cf0));
        g_have_camera = true;
    }
    MrpMoby m;
    if (!rt) {  // (the view the loop uses now against the frame's)
        auto same = [](const MrpVec4& a, const MrpVec4& b, size_t n) { return std::memcmp(&a, &b, n) == 0; };
        bool s = same(f.cam_pos, vec4_at(OV(0x00166dc0)), 12);
        for (uint32_t i = 0; s && i < 4; ++i) s = same(i < 3 ? f.view_rows[i] : f.view_trans, vec4_at(OV(0x00166cc0) + 16 * i), 16);
        m.other_view = !s;
    }
    m.moby = moby;
    m.class_ptr = lwu(moby + 0x24);
    m.class_id = lh(moby + 0xa6);
    m.flags = lhu(moby + 0x34);
    m.state = lbu(moby + 0x20);
    m.far_lod = far_lod != 0;
    m.deferred = deferred != 0;
    for (int i = 0; i < 3; ++i) m.pos[i] = lwc1(moby + 0x10 + 4 * static_cast<uint32_t>(i));
    m.scale = lwc1(moby + 0x2c);
    for (uint32_t i = 0; i < 3; ++i) m.rot[i] = vec4_at(moby + 0xc0 + 0x10 * i);
    m.light_set_a = lbu(moby + 0x38);
    m.light_set_b = lbu(moby + 0x39);
    m.light_blend = lbu(moby + 0x3a);
    m.alpha_base = lbu(moby + 0x23);
    if (rt) {
        m.rt = true;
        for (uint32_t i = 0; i < 6; ++i) m.rt_proj[i] = vec4_at(OV(0x0016ca40) + 16 * i);
        m.rt_w = view_w;
        m.rt_h = view_h;
    }
    m.chain_begin = chain_begin;
    m.chain_end = chain_end;
    m.ring = ring;
    m.record_qwords = qwords;
    m.record_first = static_cast<uint32_t>(f.records.size());
    f.records.resize(f.records.size() + static_cast<size_t>(qwords) * 16);
    for (uint32_t i = 0; i < qwords * 16; i += 4) {
        const uint32_t w = lwu(spr_rec + i);
        std::memcpy(&f.records[m.record_first + i], &w, 4);
    }
    if (parts != nullptr) {  // the MRP-only pass: its parts as the chain would have had them (with the verify: kept aside)
        const uint32_t mi = static_cast<uint32_t>(f.mobys.size());
        if (g_moby_pass_mrp_verify) {
            direct_parts(mi, parts, nparts, f.check_parts, 0, m.check_first, m.check_model_to_view);
            m.check_count = nparts;
        } else {
            m.direct = true;
            direct_parts(mi, parts, nparts, f.parts, static_cast<uint32_t>(f.unpacks.size()), m.part_first, m.model_to_view);
            m.part_count = nparts;
        }
    }
    f.mobys.push_back(m);
    if (g_prof) g_prof_obs_ms += now_ms() - t0;
}

// orig: (host) the pose observer (0x002558e0, after 0x250fe0): the joint matrices the skinning reads
void on_pose(uint32_t rec, uint32_t /*work*/) {
    const double t0 = g_prof ? now_ms() : 0.0;
    MrpFrame& f = g_frame;
    // the skinning goes through the ring in the order the draw loop wrote it
    size_t idx = f.mobys.size();
    if (g_next_pose < f.mobys.size() && f.mobys[g_next_pose].ring == rec && f.mobys[g_next_pose].joint_count == 0) idx = g_next_pose;
    else
        for (size_t i = f.mobys.size(); i-- > 0;)
            if (f.mobys[i].ring == rec && f.mobys[i].joint_count == 0) {
                idx = i;
                break;
            }
    g_last_pose = ~size_t(0);
    if (idx == f.mobys.size()) return;  // (a pose of a record the draw loop did not report: counted by the check)
    g_last_pose = idx;
    g_next_pose = idx + 1;
    MrpMoby& m = f.mobys[idx];
    const int32_t n = lh(rec + 0x10);  // the joint count (0: one identity matrix, 0x250fe0)
    const uint32_t count = n <= 0 ? 1u : static_cast<uint32_t>(n);
    m.joint_first = static_cast<uint32_t>(f.joints.size());
    m.joint_count = count;
    for (uint32_t j = 0; j < count; ++j) f.joints.push_back(mat4_at(SPR + 0x40 * j));
    if (g_prof) g_prof_obs_ms += now_ms() - t0;
}

// orig: (host) the MRP skip of the skinning (moby_skin.h): the record after `rec` -- the draw loop's record
// is `record_qwords` quadwords in the ring and the next follows it (proved: RAC1_MRP_SKIP=verify runs the skinning too
// and compares what it returns). 0 when the pose seen last was not `rec`'s.
uint32_t skip_next(uint32_t rec) {
    if (g_last_pose >= g_frame.mobys.size()) return 0;
    const MrpMoby& m = g_frame.mobys[g_last_pose];
    if (m.ring != rec || m.record_qwords == 0) return 0;
    return rec + 16 * m.record_qwords;
}

// ---- chain decoding ------------------------------------------------------------------------

// orig: (host) the bytes of data a VIF code takes (the cycle setting cl / wl); -1 unknown
int64_t vif_bytes(uint32_t code, uint32_t cl, uint32_t wl) {
    const uint32_t cmd = (code >> 24) & 0x7f, num = (code >> 16) & 0xff;
    if (cmd == 0x20) return 4;
    if (cmd == 0x30 || cmd == 0x31) return 16;
    if (cmd == 0x4a) return static_cast<int64_t>(num == 0 ? 256 : num) * 8;
    if (cmd == 0x50 || cmd == 0x51) return static_cast<int64_t>((code & 0xffff) == 0 ? 65536 : (code & 0xffff)) * 16;
    if (cmd >= 0x60) {
        const uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3;
        uint32_t cnt = num == 0 ? 256 : num;
        if (wl == 0) wl = 1;
        if (wl > cl) cnt = cl * (cnt / wl) + std::min(cnt % wl, cl);
        const uint32_t bits = (vn == 3 && vl == 3) ? 16 : (32u >> vl) * (vn + 1);
        return static_cast<int64_t>(((cnt * bits + 31) / 32) * 4);
    }
    return 0;
}

// the end of the EE memory a chain may be in: the game's 32 MB, or the host's 64 MB while the moby shadow run's chain is
// decoded (its packets in the host region mrp_lod gives it)
uint32_t g_ram_end = 0x02000000u;

struct DecodeStats {
    uint64_t chains = 0, tags = 0, overruns = 0, open_parts = 0, unusual_vif = 0, guard = 0, bad_tags = 0;
} g_dec;

// orig: (host) decodes one moby's chain parts [chain_begin, chain_end): DMA tags (CNT / NEXT / REF / REFS / END) and the VIF
// codes in their VIF words and payloads; its parts and unpacks appended to `parts` / `unpacks` (the moby's part_first and its
// parts' unpack_first index them: the frame's own, or a job's chunk's, merged into the frame after: decode_all)
void decode_chain_to(MrpMoby& m, uint32_t mi, std::vector<MrpPart>& parts, std::vector<MrpUnpack>& unpacks, DecodeStats& dec) {
    ++dec.chains;
    m.part_first = static_cast<uint32_t>(parts.size());
    MrpPart cur;
    cur.unpack_first = static_cast<uint32_t>(unpacks.size());
    cur.moby_index = mi;
    uint32_t cl = 1, wl = 1;
    bool have_matrix = false;
    MrpMat4 last_mat[2];  // the matrices uploaded to 0x129 / 0x28d so far
    bool have_last[2] = {false, false};
    uint16_t own_dest = 0;
    uint32_t a = m.chain_begin;
    for (int guard = 0; a < m.chain_end; ++guard) {
        if (guard > 4096) {
            ++dec.guard;
            break;
        }
        ++dec.tags;
        if (a > g_ram_end - 16 && (a < 0x70000000u || a > 0x70004000u - 16)) {  // (a NEXT's garbage address: see below)
            ++dec.bad_tags;
            break;
        }
        const uint32_t w0 = lwu(a), w1 = lwu(a + 4);
        const uint32_t id = (w0 >> 28) & 7, qwc = w0 & 0xffff;
        uint32_t payload = 0, next = 0;
        bool end = false;
        switch (id) {
        case 0: payload = w1; end = true; break;           // REFE
        case 1: payload = a + 16; next = a + 16 + qwc * 16; break;  // CNT
        case 2: payload = a + 16; next = w1; break;        // NEXT
        case 3: case 4: payload = w1; next = a + 16; break;  // REF / REFS
        default: payload = a + 16; end = true; break;      // END and the rest
        }
        // (the chain overwritten before the frame's end: the vendor screens' draw 0x2a02b0 kicks the frame
        // itself, then stores the frame's image (0x1f99c0) over the packet buffer, so a tag read here may be pixels)
        const uint64_t span = static_cast<uint64_t>(payload) + static_cast<uint64_t>(qwc) * 16;
        const bool ram = span <= g_ram_end, spr = payload >= 0x70000000u && span <= 0x70004000u;
        if (!ram && !spr) {
            ++dec.bad_tags;
            break;
        }
        int64_t left = static_cast<int64_t>(qwc) * 16;
        bool took = false;  // a UNPACK / DIRECT took its data from this tag's payload
        auto take = [&](int64_t bytes) -> uint32_t {
            const uint32_t at = payload;
            if (bytes > left) {
                ++dec.overruns;
                bytes = left;
            }
            payload += static_cast<uint32_t>(bytes);
            left -= bytes;
            took = true;
            return at;
        };
        auto vif = [&](uint32_t c) {
            const uint32_t cmd = (c >> 24) & 0x7f, imm = c & 0xffff, num = (c >> 16) & 0xff;
            if (cmd == 1) {
                cl = imm & 0xff;
                wl = (imm >> 8) & 0xff;
            } else if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) {  // MSCAL / MSCALF / MSCNT: the part ends
                cur.mscal = static_cast<uint16_t>(imm);
                {
                    const uint32_t pi = static_cast<uint32_t>(parts.size()) - m.part_first;  // the part's index in the moby
                    if (own_dest != 0) cur.matrix_dest = own_dest;
                    else if (pi >= 2) cur.matrix_dest = parts[parts.size() - 2].matrix_dest;
                    else if (pi == 1) cur.matrix_dest = parts[parts.size() - 1].matrix_dest == 0x129 ? 0x28d : 0x129;
                    else cur.matrix_dest = 0x129;
                    const int bi = cur.matrix_dest == 0x28d ? 1 : 0;
                    if (have_last[bi]) cur.model_to_view = last_mat[bi];
                    own_dest = 0;
                }
                cur.unpack_count = static_cast<uint32_t>(unpacks.size()) - cur.unpack_first;
                parts.push_back(cur);
                cur = MrpPart();
                cur.unpack_first = static_cast<uint32_t>(unpacks.size());
                cur.moby_index = mi;
            } else if (cmd == 0x50 || cmd == 0x51) {  // DIRECT / DIRECTHL: the GS state packet
                const int64_t b = vif_bytes(c, cl, wl);
                cur.has_gs_state = true;
                cur.direct_qwc = static_cast<uint32_t>(b / 16);
                cur.direct_data = take(b);
            } else if (cmd >= 0x60) {
                MrpUnpack u;
                u.cmd = static_cast<uint8_t>(cmd);
                u.tops = (imm & 0x8000) != 0;
                u.usn = (imm & 0x4000) != 0;
                u.dest = static_cast<uint16_t>(imm & 0x3ff);
                u.num = static_cast<uint16_t>(num == 0 ? 256 : num);
                u.cl = static_cast<uint8_t>(cl);
                u.wl = static_cast<uint8_t>(wl);
                u.data = take(vif_bytes(c, cl, wl));
                u.is_matrix = cmd == 0x6c && u.num == 4 && !u.tops && (u.dest == 0x129 || u.dest == 0x28d);
                if (u.is_matrix) {
                    if (!have_matrix) {
                        m.model_to_view = mat4_at(u.data);
                        have_matrix = true;
                    }
                    const int bi = u.dest == 0x28d ? 1 : 0;
                    last_mat[bi] = mat4_at(u.data);
                    have_last[bi] = true;
                    own_dest = u.dest;
                }
                unpacks.push_back(u);
            } else if (cmd == 0x20 || cmd == 0x30 || cmd == 0x31 || cmd == 0x4a || cmd == 5 || cmd == 2 || cmd == 3 || cmd == 4) {
                ++dec.unusual_vif;  // STMASK / STROW / STCOL / MPG / STMOD / OFFSET / BASE / ITOP: not in the moby chains
                take(vif_bytes(c, cl, wl));
            }
        };
        vif(lwu(a + 8));
        vif(lwu(a + 12));
        // VIF codes in the payload (the templates the loop copies carry some). What is left of the last quadword
        // after a UNPACK's data (< 16 bytes) is padding, not codes: whatever the skinning wrote there, or the
        // class data's (the skinning skipped leaves stale bytes in the chain's padding)
        while (left >= 4 && !(took && left < 16)) {
            const uint32_t c = lwu(payload);
            payload += 4;
            left -= 4;
            vif(c);
        }
        if (end) break;
        a = next;
    }
    if (cur.unpack_first != unpacks.size()) ++dec.open_parts;
    m.part_count = static_cast<uint32_t>(parts.size()) - m.part_first;
}

void decode_chain(MrpFrame& f, uint32_t mi) { decode_chain_to(f.mobys[mi], mi, f.parts, f.unpacks, g_dec); }

// ---- the record's fields ---------------------------------------------------------------------

float rf(const MrpFrame& f, const MrpMoby& m, uint32_t off) {
    float v;
    std::memcpy(&v, &f.records[m.record_first + off], 4);
    return v;
}
uint32_t ru(const MrpFrame& f, const MrpMoby& m, uint32_t off) {
    uint32_t v;
    std::memcpy(&v, &f.records[m.record_first + off], 4);
    return v;
}

// orig: (host) the record's layout, written by the draw loop (0x255d18..0x256668): +0 the packet count, +0x10 low half
// the joint count, +0x14 the shadow-mesh flag (moby +0x73), +0x40.. the lights (directions in model space as columns:
// +0x40 / +0x50 / +0x60 the first, +0x44.. the second, +0x48.. the summed point lights; colours +0x70 / +0x80 / +0x90),
// +0xa0 / +0xa4 the colour rows' w lanes, +0xb0 the tint (four floats 65536 + byte), +0xc0.. the parts (word, chain address)
void decode_record(MrpFrame& f, MrpMoby& m) {
    if (m.record_qwords < 0xc) return;
    m.packet_count = ru(f, m, 0);
    m.animated = (ru(f, m, 0x10) & 0xff) != 0;
    m.shadow_mesh = ru(f, m, 0x14) != 0;
    for (uint32_t c = 0; c < 3; ++c) {
        m.light_dir[c] = MrpVec4{rf(f, m, 0x40 + 4 * c), rf(f, m, 0x50 + 4 * c), rf(f, m, 0x60 + 4 * c), 0.0f};
        m.light_col[c] = MrpVec4{rf(f, m, 0x70 + 0x10 * c), rf(f, m, 0x74 + 0x10 * c), rf(f, m, 0x78 + 0x10 * c), rf(f, m, 0x7c + 0x10 * c)};
    }
    m.light_w[0] = rf(f, m, 0xa0);
    m.light_w[1] = rf(f, m, 0xa4);
    for (uint32_t i = 0; i < 4; ++i) m.tint[i] = static_cast<uint8_t>(ru(f, m, 0xb0 + 4 * i) & 0xff);
    m.first_packet = m.far_lod && m.class_ptr != 0 ? lbu(m.class_ptr + 4) : 0;
}

// The chunks of mobys decode_all gives a job: each chunk's parts and unpacks (its own indices) and its counts
struct DecodeChunk {
    std::vector<MrpPart> parts;
    std::vector<MrpUnpack> unpacks;
    DecodeStats st;
};
std::vector<DecodeChunk> g_decode_chunks;  // (kept: their vectors' room reused frame after frame)
constexpr size_t kDecodeGrain = 16;

// orig: (host) every moby's record and chain decoded into the frame (its parts and unpacks in the mobys' order). The chains
// are only read (the game's memory, or the shadow run's), so the mobys are decoded on the job pool (src/jobs), a chunk of
// them per job, each into its own vectors, then appended in the chunks' order: the same frame as one decode after another
// (RAC1_JOBS=1), on any number of threads.
void decode_all(MrpFrame& f) {
    const size_t n = f.mobys.size();
    const size_t nc = jobs::chunks(n, kDecodeGrain);
    bool any_direct = false;  // (the MRP-only pass's mobys: their parts given, only their records decoded)
    for (const MrpMoby& m : f.mobys) any_direct = any_direct || m.direct;
    if (jobs::threads() <= 1 || nc <= 1 || any_direct) {
        for (uint32_t i = 0; i < n; ++i) {
            decode_record(f, f.mobys[i]);
            if (!f.mobys[i].direct) decode_chain(f, i);
        }
        return;
    }
    if (g_decode_chunks.size() < nc) g_decode_chunks.resize(nc);
    jobs::parallel_for(n, kDecodeGrain, [&f](size_t b, size_t e) {
        DecodeChunk& c = g_decode_chunks[b / kDecodeGrain];
        c.parts.clear();
        c.unpacks.clear();
        c.st = DecodeStats{};
        for (size_t i = b; i < e; ++i) {
            decode_record(f, f.mobys[i]);
            decode_chain_to(f.mobys[i], static_cast<uint32_t>(i), c.parts, c.unpacks, c.st);
        }
    });
    for (size_t k = 0; k < nc; ++k) {
        DecodeChunk& c = g_decode_chunks[k];
        const uint32_t pbase = static_cast<uint32_t>(f.parts.size()), ubase = static_cast<uint32_t>(f.unpacks.size());
        for (size_t i = k * kDecodeGrain; i < std::min(n, (k + 1) * kDecodeGrain); ++i) f.mobys[i].part_first += pbase;
        for (MrpPart& p : c.parts) p.unpack_first += ubase;
        f.parts.insert(f.parts.end(), c.parts.begin(), c.parts.end());
        f.unpacks.insert(f.unpacks.end(), c.unpacks.begin(), c.unpacks.end());
        g_dec.chains += c.st.chains;
        g_dec.tags += c.st.tags;
        g_dec.overruns += c.st.overruns;
        g_dec.open_parts += c.st.open_parts;
        g_dec.unusual_vif += c.st.unusual_vif;
        g_dec.guard += c.st.guard;
        g_dec.bad_tags += c.st.bad_tags;
    }
}

// orig: (host) testing, RAC1_MOBY_PASS_MRP=verify: the parts the draw loop gave (MrpFrame::check_parts) against the ones decoded
// from the chain it also wrote (the MSCAL, the matrix's buffer and rows, the GS state as the renderer reads it), the moby's first
// matrix; the totals every 200 frames, the first mismatches logged
struct DirectStats {
    uint64_t frames = 0, mobys = 0, parts = 0, bad_mobys = 0, bad_parts = 0, gs_parts = 0, shadow_mesh = 0;
} g_direct;
void direct_verify(const MrpFrame& f) {
    if (!g_moby_pass_mrp_verify) return;
    ++g_direct.frames;
    for (const MrpMoby& m : f.mobys) {
        ++g_direct.mobys;
        if (m.shadow_mesh) ++g_direct.shadow_mesh;
        const bool count_ok = m.check_count == m.part_count;
        const bool m2v_ok = std::memcmp(&m.check_model_to_view, &m.model_to_view, sizeof m.model_to_view) == 0;
        if (!count_ok || !m2v_ok) {
            if (++g_direct.bad_mobys <= 10)
                std::fprintf(stderr, "[moby-pass-mrp] verify: moby %08x class %d: %u parts given, %u decoded%s\n", m.moby, m.class_id, m.check_count, m.part_count,
                             m2v_ok ? "" : ", its matrix differs");
            if (!count_ok) continue;
        }
        for (uint32_t k = 0; k < m.part_count; ++k) {
            ++g_direct.parts;
            const MrpPart& g = f.check_parts[m.check_first + k];
            MrpPart d = f.parts[m.part_first + k];
            if (!d.gs_resolved && d.has_gs_state) gs_resolve_at(d, d.direct_data, d.direct_qwc);
            if (d.has_gs_state) ++g_direct.gs_parts;
            const bool ok = g.mscal == d.mscal && g.matrix_dest == d.matrix_dest && std::memcmp(&g.model_to_view, &d.model_to_view, sizeof g.model_to_view) == 0 &&
                            g.has_gs_state == d.has_gs_state && g.has_test == d.has_test && g.has_alpha == d.has_alpha && (!g.has_test || g.test == d.test) &&
                            (!g.has_alpha || g.alpha == d.alpha);
            if (!ok && ++g_direct.bad_parts <= 10)
                std::fprintf(stderr, "[moby-pass-mrp] verify: moby %08x class %d part %u: mscal %03x/%03x dest %03x/%03x gs %d/%d test %llx/%llx alpha %llx/%llx\n", m.moby,
                             m.class_id, k, g.mscal, d.mscal, g.matrix_dest, d.matrix_dest, g.has_gs_state, d.has_gs_state, static_cast<unsigned long long>(g.test),
                             static_cast<unsigned long long>(d.test), static_cast<unsigned long long>(g.alpha), static_cast<unsigned long long>(d.alpha));
        }
    }
    if (g_direct.frames % 200 == 0)
        std::printf("[moby-pass-mrp] verify: %llu frames, %llu mobys (%llu differ; %llu with the shadow mesh), %llu parts (%llu differ; %llu with a GS state)\n",
                    static_cast<unsigned long long>(g_direct.frames), static_cast<unsigned long long>(g_direct.mobys), static_cast<unsigned long long>(g_direct.bad_mobys),
                    static_cast<unsigned long long>(g_direct.shadow_mesh), static_cast<unsigned long long>(g_direct.parts), static_cast<unsigned long long>(g_direct.bad_parts),
                    static_cast<unsigned long long>(g_direct.gs_parts));
}

// orig: (host) testing, RAC1_MRP_SCENE_HASH=frame: one hash per game frame of what the scene's users read (not the unpacks, which
// only the debug checks read, nor where the GS state packet is: its values), to compare two runs frame for frame
uint64_t scene_frame_hash(const MrpFrame& d) {
    uint64_t h = 0xcbf29ce484222325ull;
    auto add = [&h](const void* p, size_t k) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < k; ++i) h = (h ^ b[i]) * 0x100000001b3ull;
    };
    auto add_u = [&add](uint64_t v) { add(&v, 8); };
    add(d.proj, sizeof d.proj);
    add(&d.cam_pos, sizeof d.cam_pos);
    add(d.view_rows, sizeof d.view_rows);
    add(&d.view_trans, sizeof d.view_trans);
    add_u(d.mobys.size());
    for (const MrpMoby& m : d.mobys) {
        add_u(m.moby);
        add_u(m.class_ptr);
        add_u(static_cast<uint64_t>(m.class_id) << 32 | static_cast<uint64_t>(m.flags) << 16 | m.state);
        add_u(static_cast<uint64_t>(m.far_lod) | m.deferred << 1 | m.shadow_mesh << 2 | m.animated << 3 | m.other_view << 4 | m.rt << 5);
        add_u(static_cast<uint64_t>(m.first_packet) << 32 | m.packet_count);
        add(m.pos, sizeof m.pos);
        add(&m.scale, 4);
        add(m.rot, sizeof m.rot);
        add(&m.model_to_view, sizeof m.model_to_view);
        add(m.light_dir, sizeof m.light_dir);
        add(m.light_col, sizeof m.light_col);
        add(m.light_w, sizeof m.light_w);
        add(m.tint, 4);
        add_u(static_cast<uint64_t>(m.alpha_base) | m.light_set_a << 8 | m.light_set_b << 16 | m.light_blend << 24);
        if (m.rt) {
            add(m.rt_proj, sizeof m.rt_proj);
            add_u(static_cast<uint64_t>(static_cast<uint32_t>(m.rt_w)) << 32 | static_cast<uint32_t>(m.rt_h));
        }
        add_u(m.joint_count);
        if (m.joint_count != 0) add(&d.joints[m.joint_first], sizeof(MrpMat4) * m.joint_count);
        add_u(static_cast<uint64_t>(m.chain_begin) << 32 | m.chain_end);
        add_u(static_cast<uint64_t>(m.ring) << 32 | m.record_qwords);
        if (m.record_qwords != 0) add(&d.records[m.record_first], 16u * m.record_qwords);
        add_u(m.part_count);
        for (uint32_t k = 0; k < m.part_count; ++k) {
            MrpPart p = d.parts[m.part_first + k];
            if (!p.gs_resolved && p.has_gs_state) gs_resolve_at(p, p.direct_data, p.direct_qwc);
            add_u(static_cast<uint64_t>(p.mscal) << 32 | static_cast<uint64_t>(p.matrix_dest) << 16 | p.has_gs_state);
            add_u(p.moby_index);
            add(&p.model_to_view, sizeof p.model_to_view);
            add_u(static_cast<uint64_t>(p.has_test) | p.has_alpha << 1);
            add_u(p.has_test ? p.test : 0);
            add_u(p.has_alpha ? p.alpha : 0);
        }
    }
    return h;
}

// ---- the differential check ------------------------------------------------------------------

struct CheckStats {
    uint64_t frames = 0, scene_mobys = 0, scene_parts = 0;
    uint64_t starts = 0, part_starts = 0, other_starts = 0;
    uint64_t matrix_cmp = 0, proj_cmp = 0, unpack_cmp = 0, unpack_qw = 0, mscal_cmp = 0;
    uint64_t record_cmp = 0, record_ring_diff = 0, chain_part_mismatch = 0, pose_missing = 0, pose_unmatched = 0;
    uint64_t drawn_flag_zero = 0, packet_count_cmp = 0, packet_count_diff = 0, list_cmp = 0, list_flag_not_drawn = 0, light_cmp = 0, light_odd = 0;
    uint64_t matrix_dup_cmp = 0, matrix_dup_diff = 0, matrix_dup_diff_shadow = 0;
    uint64_t frames_start_count_diff = 0, frames_no_chain = 0, frames_prev_chain = 0;
    std::map<int, uint64_t> flag_classes;      // info: moby class (+10000: drop shadow caster) -> flagged visible (+0x31) but not drawn
    std::map<std::string, uint64_t> classes;  // mismatch class -> count
    std::map<std::string, std::string> examples;
} g_cs;
bool g_chain_runs = true, g_chain_previous = false;
uint32_t g_k = 0;      // the part start counter of the chain
uint32_t g_frame_starts_other = 0;

void mismatch(const std::string& cls, const std::string& detail) {
    if (g_cs.classes[cls]++ == 0) g_cs.examples[cls] = detail;
    static int logged = 0;
    if (logged < 12) {
        ++logged;
        std::fprintf(stderr, "[mrp-check] %s: %s\n", cls.c_str(), detail.c_str());
    }
}

// testing (RAC1_MRP_SCENE_CHECK=selftest): the expectation of every 1000th vector perturbed: the check must report it
const bool g_selftest = g_check && std::strcmp(std::getenv("RAC1_MRP_SCENE_CHECK"), "selftest") == 0;
uint64_t g_selftest_n = 0;

bool qw_equal(const uint8_t* mem, uint32_t qw, const uint32_t* expect, int lanes = 4) {
    if (g_selftest && ++g_selftest_n % 1000 == 0) return false;
    return std::memcmp(&mem[(qw & 0x3ff) << 4], expect, 4 * static_cast<size_t>(lanes)) == 0;
}

// orig: (host) what a VIF UNPACK writes for vector `n` of the unpack `u` (lanes beyond the format's are set to 0xffffffff in
// `valid` = 0: not compared)
bool unpack_vector(const MrpUnpack& u, uint32_t n, uint32_t out[4], uint32_t& mask) {
    const uint32_t vn = (u.cmd >> 2) & 3, vl = u.cmd & 3;
    mask = 0xf;
    if (vn == 3 && vl == 3) {  // V4-5
        const uint16_t h = ee_read<uint16_t>(u.data + 2 * n);
        out[0] = (h & 0x1fu) << 3;
        out[1] = ((h >> 5) & 0x1fu) << 3;
        out[2] = ((h >> 10) & 0x1fu) << 3;
        out[3] = (h >> 15) << 7;
        return true;
    }
    const uint32_t ebits = 32u >> vl, comps = vn + 1;
    uint32_t e[4] = {0, 0, 0, 0};
    for (uint32_t k = 0; k < comps; ++k) {
        const uint64_t bit = (static_cast<uint64_t>(n) * comps + k) * ebits;
        const uint32_t byte = u.data + static_cast<uint32_t>(bit >> 3);
        if (ebits == 32) e[k] = ee_read<uint32_t>(byte);
        else if (ebits == 16) {
            const uint16_t h = ee_read<uint16_t>(byte);
            e[k] = u.usn ? h : static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(h)));
        } else {
            const uint8_t b = ee_read<uint8_t>(byte);
            e[k] = u.usn ? b : static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(b)));
        }
    }
    for (uint32_t k = 0; k < 4; ++k) out[k] = e[k];
    if (vn == 0) mask = 0x1;       // S: x
    else if (vn == 1) mask = 0x3;  // V2: x, y
    else if (vn == 2) mask = 0x7;  // V3: w undefined
    return true;
}

}  // namespace

// ---- public ----------------------------------------------------------------------------------

void mrp_scene_install() {
    g_moby_draw_observer = on_draw;
    g_moby_pose_observer = on_pose;
    g_moby_skip_next = skip_next;
}

const MrpFrame& mrp_scene() { return g_frame; }
const MrpFrame& mrp_scene_draw() { return g_shadow_valid ? g_shadow_frame : g_frame; }

// orig: (host) see mrp_scene.h
void mrp_scene_shadow_begin() {
    g_kept_frame = std::move(g_frame);
    g_frame = MrpFrame();
    g_next_pose = 0;
    g_last_pose = ~size_t(0);
    g_have_camera = false;
}

// orig: (host) see mrp_scene.h
void mrp_scene_shadow_end() {
    MrpFrame& f = g_frame;
    g_ram_end = 0x04000000u;
    decode_all(f);
    g_ram_end = 0x02000000u;
    direct_verify(f);
    for (MrpPart& p : f.parts) {  // the GS state packet (A+D, PACKED: the frame's TEST_1 0x47 / ALPHA_1 0x42; the MRP-only pass's: resolved)
        if (p.gs_resolved) continue;
        p.gs_resolved = true;
        if (p.has_gs_state) gs_resolve_at(p, p.direct_data, p.direct_qwc);  // (as mrp_moby_renderer.cpp's gif_ad reads it at the draw)
    }
    g_shadow_frame = std::move(f);
    g_shadow_valid = true;
    g_frame = std::move(g_kept_frame);
    g_kept_frame = MrpFrame();
    g_next_pose = 0;
    g_last_pose = ~size_t(0);
    g_have_camera = !g_frame.mobys.empty();
}

namespace {
// the mobys the real pass drew that the shadow run's scene (the LOD slider / the widened view) has not: counted always,
// logged with RAC1_MRP_LOD_DROPS=1 (the first 40, then one line per 600 frames)
struct DropStats {
    uint64_t frames = 0, real = 0, shadow = 0, dropped = 0, frames_dropping = 0, incomplete = 0, compared = 0, differing = 0, kept = 0, after_missing = 0, other_model = 0, other_model_same_pose = 0;
} g_drops;
void lod_drop_check(const MrpFrame& real, const MrpFrame& shadow) {
    static const bool log = std::getenv("RAC1_MRP_LOD_DROPS") != nullptr;
    ++g_drops.frames;
    g_drops.real += real.mobys.size();
    g_drops.shadow += shadow.mobys.size();
    static std::vector<std::pair<uint32_t, uint32_t>> at;
    at.clear();
    at.reserve(shadow.mobys.size());
    size_t shadow_deferred = 0;
    for (uint32_t i = 0; i < shadow.mobys.size(); ++i) {
        const MrpMoby& m = shadow.mobys[i];
        at.emplace_back(m.moby, i);
        shadow_deferred += m.deferred ? 1 : 0;
        if (m.joint_count == 0 || m.part_count == 0) {
            ++g_drops.incomplete;
            if (log && g_drops.incomplete <= 40)
                std::fprintf(stderr, "[lod-drop] frame %d: LOD scene moby %08x class %d with %u joints, %u parts\n", g_frame_no, m.moby, m.class_id, m.joint_count, m.part_count);
        }
    }
    std::sort(at.begin(), at.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    uint64_t n = 0;
    for (const MrpMoby& r : real.mobys) {
        const auto it = std::lower_bound(at.begin(), at.end(), r.moby, [](const auto& a, uint32_t k) { return a.first < k; });
        if (it == at.end() || it->first != r.moby) {
            ++n;
            if (log && g_drops.dropped + n <= 40) {
                const float dx = r.pos[0] - real.cam_pos.x, dy = r.pos[1] - real.cam_pos.y, dz = r.pos[2] - real.cam_pos.z;
                std::fprintf(stderr, "[lod-drop] frame %d: moby %08x class %d state %d flags %04x far %d deferred %d +0x32 %d +0x72 %d dist %.1f (real %zu, shadow %zu, of them deferred %zu)\n",
                             g_frame_no, r.moby, r.class_id, r.state, r.flags, r.far_lod ? 1 : 0, r.deferred ? 1 : 0, lh(r.moby + 0x32), lbu(r.moby + 0x72),
                             std::sqrt(dx * dx + dy * dy + dz * dz), real.mobys.size(), shadow.mobys.size(), shadow_deferred);
            }
            continue;
        }
        const MrpMoby& s = shadow.mobys[it->second];
        if (r.far_lod != s.far_lod) {  // (info: the pose of the other model)
            ++g_drops.other_model;
            if (r.joint_count == s.joint_count &&
                (r.joint_count == 0 || std::memcmp(&real.joints[r.joint_first], &shadow.joints[s.joint_first], sizeof(MrpMat4) * r.joint_count) == 0))
                ++g_drops.other_model_same_pose;
            continue;
        }
        ++g_drops.compared;
        const bool pose = r.joint_count == s.joint_count &&
                          (r.joint_count == 0 || std::memcmp(&real.joints[r.joint_first], &shadow.joints[s.joint_first], sizeof(MrpMat4) * r.joint_count) == 0);
        const bool mtv = std::memcmp(&r.model_to_view, &s.model_to_view, sizeof(MrpMat4)) == 0;
        const bool parts = r.part_count == s.part_count;
        if (!pose || !mtv || !parts) {
            ++g_drops.differing;
            if (log && g_drops.differing <= 40)
                std::fprintf(stderr, "[lod-drop] frame %d: moby %08x class %d differs: pose %s (%u / %u joints), model-to-view %s, parts %u / %u\n", g_frame_no, r.moby, r.class_id,
                             pose ? "same" : "DIFFERS", r.joint_count, s.joint_count, mtv ? "same" : "DIFFERS", r.part_count, s.part_count);
        }
    }
    g_drops.dropped += n;
    g_drops.frames_dropping += n != 0 ? 1 : 0;
    if (log && g_drops.frames % 600 == 0)
        std::fprintf(stderr, "[lod-drop] %llu frames: %llu mobys of the real pass missing from the LOD scene (%llu frames), %.1f real / %.1f LOD mobys a frame\n",
                     static_cast<unsigned long long>(g_drops.frames), static_cast<unsigned long long>(g_drops.dropped), static_cast<unsigned long long>(g_drops.frames_dropping),
                     static_cast<double>(g_drops.real) / static_cast<double>(g_drops.frames), static_cast<double>(g_drops.shadow) / static_cast<double>(g_drops.frames));
}
// The shadow run is the same pass with more to draw, and the pass has fixed capacities: 48 deferred mobys (those with a
// drop shadow, SPR 0x3000..0x3300: "the deferred list full", 0x2568f0 -> not drawn), 64 sub-mesh records (SPR 0x3400..0x3800), the
// packet buffer (0x1611c8). Once the widened / scaled view fills one, the mobys after it in the list are not drawn by the shadow -
// mobys near the camera (Ratchet) included, which the real pass draws. Every moby the real pass drew and the shadow did not is
// appended to the drawn scene as the real pass drew it (its pose, matrix, record, parts; the parts' GS state read from the real
// chain at the draw, as for the real scene). RAC1_MRP_LOD_KEEP_REAL=0: off (testing: the drops seen).
void lod_keep_real(const MrpFrame& real, MrpFrame& draw) {
    static const bool off = [] {
        const char* v = std::getenv("RAC1_MRP_LOD_KEEP_REAL");
        return v != nullptr && std::atoi(v) == 0;
    }();
    if (off) return;
    static std::vector<uint32_t> have;
    have.clear();
    have.reserve(draw.mobys.size());
    for (const MrpMoby& m : draw.mobys) have.push_back(m.moby);
    std::sort(have.begin(), have.end());
    for (const MrpMoby& r : real.mobys) {
        if (std::binary_search(have.begin(), have.end(), r.moby)) continue;
        MrpMoby m = r;
        const uint32_t mi = static_cast<uint32_t>(draw.mobys.size());
        m.joint_first = static_cast<uint32_t>(draw.joints.size());
        draw.joints.insert(draw.joints.end(), real.joints.begin() + r.joint_first, real.joints.begin() + r.joint_first + r.joint_count);
        m.record_first = static_cast<uint32_t>(draw.records.size());
        draw.records.insert(draw.records.end(), real.records.begin() + r.record_first, real.records.begin() + r.record_first + static_cast<size_t>(r.record_qwords) * 16);
        m.part_first = static_cast<uint32_t>(draw.parts.size());
        for (uint32_t k = 0; k < r.part_count; ++k) {
            MrpPart p = real.parts[r.part_first + k];
            const uint32_t u0 = p.unpack_first;
            p.unpack_first = static_cast<uint32_t>(draw.unpacks.size());
            draw.unpacks.insert(draw.unpacks.end(), real.unpacks.begin() + u0, real.unpacks.begin() + u0 + p.unpack_count);
            p.moby_index = mi;
            draw.parts.push_back(p);
        }
        draw.mobys.push_back(m);
        ++g_drops.kept;
    }
}
}  // namespace

// orig: (host) the drop check's totals (mobys of the real pass the LOD scene missed)
void mrp_scene_lod_drop_report() {
    if (g_drops.frames == 0) return;
    std::printf("[lod] moby scenes compared %llu: real-pass mobys missing from the LOD scene %llu (in %llu frames); %.1f real / %.1f LOD mobys a frame; LOD mobys "
                "without a pose or parts %llu; same-model mobys compared %llu, pose / matrix / parts differing %llu; kept from the real pass %llu, missing after that %llu\n",
                static_cast<unsigned long long>(g_drops.frames), static_cast<unsigned long long>(g_drops.dropped), static_cast<unsigned long long>(g_drops.frames_dropping),
                static_cast<double>(g_drops.real) / static_cast<double>(g_drops.frames), static_cast<double>(g_drops.shadow) / static_cast<double>(g_drops.frames),
                static_cast<unsigned long long>(g_drops.incomplete), static_cast<unsigned long long>(g_drops.compared), static_cast<unsigned long long>(g_drops.differing),
                static_cast<unsigned long long>(g_drops.kept), static_cast<unsigned long long>(g_drops.after_missing));
    std::printf("[lod] mobys both drew with the other model (near / far) %llu, of them with the same pose %llu\n", static_cast<unsigned long long>(g_drops.other_model),
                static_cast<unsigned long long>(g_drops.other_model_same_pose));
}

void mrp_scene_begin_frame() {
    g_shadow_valid = false;
    MrpFrame& f = g_frame;
    f.mobys.clear();
    f.parts.clear();
    f.unpacks.clear();
    f.joints.clear();
    f.records.clear();
    g_next_pose = 0;
    g_last_pose = ~size_t(0);
    g_have_camera = false;
}

void mrp_scene_end_frame() {
    const double t0 = g_prof ? now_ms() : 0.0;
    MrpFrame& f = g_frame;
    ++g_frame_no;
    decode_all(f);
    direct_verify(f);
    {  // (testing: RAC1_MRP_SCENE_HASH=frame)
        static const bool per_frame = std::getenv("RAC1_MRP_SCENE_HASH") != nullptr && std::strcmp(std::getenv("RAC1_MRP_SCENE_HASH"), "frame") == 0;
        if (per_frame) {
            std::fprintf(stderr, "[scene-hash] frame %d: %016llx (%zu mobys, %zu parts)", g_frame_no, static_cast<unsigned long long>(scene_frame_hash(f)), f.mobys.size(),
                         f.parts.size());
            if (g_shadow_valid) std::fprintf(stderr, " draw %016llx", static_cast<unsigned long long>(scene_frame_hash(g_shadow_frame)));
            std::fprintf(stderr, "\n");
        }
    }
    if (g_check) {
        ++g_cs.frames;
        g_cs.scene_mobys += f.mobys.size();
        g_cs.scene_parts += f.parts.size();
        for (const MrpMoby& m : f.mobys) {
            // the record as the skinning reads it (the ring in RAM, filled by the fromSPR) against what the observer saw
            ++g_cs.record_cmp;
            bool same = true;
            for (uint32_t i = 0; i < m.record_qwords * 16 && same; i += 4)
                if (lwu(m.ring + i) != ru(f, m, i)) same = false;
            if (!same) {
                ++g_cs.record_ring_diff;
                mismatch("record: ring in RAM differs from the record sent", "moby " + std::to_string(m.moby));
            }
            if (m.joint_count == 0) {
                ++g_cs.pose_missing;
                mismatch("pose: no joint pass seen for a moby drawn", "moby " + std::to_string(m.moby));
            }
            if (m.class_ptr != 0) {
                ++g_cs.packet_count_cmp;
                const uint32_t want = lbu(m.class_ptr + (m.far_lod ? 5 : 4));
                if (m.packet_count != want && !m.shadow_mesh) {
                    ++g_cs.packet_count_diff;
                    mismatch("record: packet count != class byte", "moby " + std::to_string(m.moby));
                }
            }
            if (!m.shadow_mesh) {  // the lights: unit directions (or zero), colours in range
                ++g_cs.light_cmp;
                bool odd = false;
                for (int c = 0; c < 3; ++c) {
                    const MrpVec4& d = m.light_dir[c];
                    const float l2 = d.x * d.x + d.y * d.y + d.z * d.z;
                    if (!(l2 == 0.0f || (l2 > 0.99f && l2 < 1.01f))) odd = true;
                    const MrpVec4& k = m.light_col[c];
                    if (!(k.x >= 0.0f && k.x <= 4.0f && k.y >= 0.0f && k.y <= 4.0f && k.z >= 0.0f && k.z <= 4.0f)) odd = true;
                }
                if (odd) {
                    ++g_cs.light_odd;
                    mismatch("light: direction not unit / colour out of range", "moby " + std::to_string(m.moby) + " class " + std::to_string(m.class_id));
                }
            }
            if (lbu(m.moby + 0x31) == 0) ++g_cs.drawn_flag_zero;
            // the matrix is sent twice (the double buffer): both must be the same
            const MrpMat4* first = nullptr;
            for (uint32_t p = 0; p < m.part_count; ++p) {
                const MrpPart& pt = f.parts[m.part_first + p];
                for (uint32_t u = 0; u < pt.unpack_count; ++u) {
                    const MrpUnpack& up = f.unpacks[pt.unpack_first + u];
                    if (!up.is_matrix) continue;
                    const MrpMat4 mm = mat4_at(up.data);
                    if (first == nullptr) first = &m.model_to_view;
                    ++g_cs.matrix_dup_cmp;
                    if (std::memcmp(&mm, first, sizeof mm) != 0) {  // (info: shadow-mesh mobys, rewritten by a later pass; the parts carry their own matrix)
                        ++g_cs.matrix_dup_diff;
                        if (m.shadow_mesh) ++g_cs.matrix_dup_diff_shadow;
                    }
                }
            }
            if (m.part_count == 0) mismatch("chain: moby with no part decoded", "moby " + std::to_string(m.moby));
        }
        // the moby list's own view: the mobys the draw loop left flagged visible (+0x31, set at 0x255e38, cleared at 0x2566c0) are
        // the mobys the scene has (both ways)
        std::map<uint32_t, bool> in_scene;
        for (const MrpMoby& m : f.mobys) in_scene[m.moby] = true;
        for (uint32_t mb = lwu(OV(0x0015ffd8)); mb < lwu(OV(0x0015ffdc)) && lbu(mb + 0x20) != 0xff; mb += 0x100) {
            const bool flagged = static_cast<int8_t>(lbu(mb + 0x20)) >= 0 && lbu(mb + 0x31) != 0;
            const bool drawn = in_scene.count(mb) != 0;
            ++g_cs.list_cmp;
            if (flagged && !drawn) {
                ++g_cs.list_flag_not_drawn;
                ++g_cs.flag_classes[(lhu(mb + 0x34) & 0xc00) != 0 ? 10000 + lh(mb + 0xa6) : lh(mb + 0xa6)];  // (info, not a mismatch)
            }
        }
    }
    if (g_shadow_valid) {
        lod_drop_check(f, g_shadow_frame);
        lod_keep_real(f, g_shadow_frame);
    }
    if (std::getenv("RAC1_MRP_SCENE_HASH") != nullptr && std::strcmp(std::getenv("RAC1_MRP_SCENE_HASH"), "frame") != 0) {  // (testing: a hash of every drawn scene, to compare two runs draw for draw)
        static uint64_t h = 0xcbf29ce484222325ull, n = 0;
        auto add = [](const void* p, size_t k) {
            const uint8_t* b = static_cast<const uint8_t*>(p);
            for (size_t i = 0; i < k; ++i) h = (h ^ b[i]) * 0x100000001b3ull;
        };
        const MrpFrame& d = mrp_scene_draw();
        for (const MrpMoby& m : d.mobys) {
            add(&m.moby, 4);
            add(&m.far_lod, 1);
            add(&m.model_to_view, sizeof m.model_to_view);
            add(&m.tint, 4);
            if (m.joint_count != 0) add(&d.joints[m.joint_first], sizeof(MrpMat4) * m.joint_count);
            if (m.record_qwords != 0) add(&d.records[m.record_first], 16u * m.record_qwords);
            for (uint32_t k = 0; k < m.part_count; ++k) {
                const MrpPart& p = d.parts[m.part_first + k];
                add(&p.mscal, 2);
                add(&p.model_to_view, sizeof p.model_to_view);
                add(&p.test, 8);
                add(&p.alpha, 8);
                for (uint32_t u = 0; u < p.unpack_count; ++u) {
                    const MrpUnpack& up = d.unpacks[p.unpack_first + u];
                    add(&up.cmd, 1);
                    add(&up.dest, 2);
                    add(&up.num, 2);
                    add(&up.data, 4);
                }
            }
        }
        if (++n % 100 == 0) std::fprintf(stderr, "[scene-hash] %llu frames: %016llx\n", static_cast<unsigned long long>(n), static_cast<unsigned long long>(h));
    }
    if (g_prof) {
        g_prof_decode_ms += now_ms() - t0;
        if (++g_prof_frames == 120) {
            std::printf("[prof] mrp_scene ms/frame: observers %.3f decode %.3f | %zu mobys %zu parts %zu unpacks %zu joints\n", g_prof_obs_ms / 120, g_prof_decode_ms / 120, f.mobys.size(),
                        f.parts.size(), f.unpacks.size(), f.joints.size());
            g_prof_obs_ms = g_prof_decode_ms = 0;
            g_prof_frames = 0;
        }
    }
}

bool mrp_scene_check_enabled() { return g_check; }

void mrp_scene_check_begin_chain(bool runs, bool previous) {
    g_chain_runs = runs;
    g_chain_previous = previous;
    if (!runs) ++g_cs.frames_no_chain;
    else if (previous) ++g_cs.frames_prev_chain;
    g_k = 0;
    g_frame_starts_other = 0;
}

void mrp_scene_check_start(const VUState& st, uint32_t start) {
    const MrpFrame& f = g_frame;
    ++g_cs.starts;
    if (g_chain_previous) return;  // (an older frame's chain: no scene to compare)
    if (start != 0xa && start != 0xe && start != 0x8 && start != 0xc) {
        ++g_cs.other_starts;
        ++g_frame_starts_other;
        return;
    }
    ++g_cs.part_starts;
    const uint32_t k = g_k++;
    if (k >= f.parts.size()) {
        mismatch("start: a VU1 moby start with no scene part", "k " + std::to_string(k));
        return;
    }
    const MrpPart& pt = f.parts[k];
    const MrpMoby& m = f.mobys[pt.moby_index];
    char buf[256];
    ++g_cs.mscal_cmp;
    if (pt.mscal != start) {
        std::snprintf(buf, sizeof buf, "part %u of moby %08x: chain MSCAL %x, VU1 start %x", k, m.moby, pt.mscal, start);
        mismatch("start: program address differs from the chain's MSCAL", buf);
    }
    // the buffer the program reads (moby_vu1.cpp 0x009 / 0x00b / 0x01a: vi13 = 0 for the starts 8 / c, xtop - 356 otherwise)
    const int32_t vi13 = (start == 0x8 || start == 0xc) ? 0 : static_cast<int32_t>(st.xtop & 0x3ff) - 356;
    const uint8_t* mem = st.mem.data();
    // projection block (6 vectors at 647 + vi13) and the model-to-view matrix (653 + vi13)
    ++g_cs.proj_cmp;
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t e[4];
        std::memcpy(e, &f.proj[i], 16);
        if (!qw_equal(mem, static_cast<uint32_t>(647 + vi13) + i, e)) {
            std::snprintf(buf, sizeof buf, "part %u moby %08x class %d: vector %u of the projection block", k, m.moby, m.class_id, i);
            mismatch("projection block: VU1 memory differs from the scene", buf);
            break;
        }
    }
    ++g_cs.matrix_cmp;
    if (static_cast<int32_t>(pt.matrix_dest) != 653 + vi13) {
        std::snprintf(buf, sizeof buf, "part %u moby %08x: buffer parity %x vs the program's %x (start %x xtop %x)", k, m.moby, pt.matrix_dest, 653 + vi13, start, st.xtop);
        mismatch("matrix buffer: the part's matrix address differs from the one the program reads", buf);
    }
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t e[4];
        std::memcpy(e, &pt.model_to_view.r[i], 16);
        if (!qw_equal(mem, static_cast<uint32_t>(653 + vi13) + i, e)) {
            std::snprintf(buf, sizeof buf, "part %u moby %08x class %d row %u (start %x xtop %x)", k, m.moby, m.class_id, i, start, st.xtop);
            mismatch("model-to-view matrix: VU1 memory differs from the scene", buf);
            break;
        }
    }
    // every UNPACK of the part: re-unpacked here from the EE data, compared with VU1 memory
    for (uint32_t u = 0; u < pt.unpack_count; ++u) {
        const MrpUnpack& up = f.unpacks[pt.unpack_first + u];
        ++g_cs.unpack_cmp;
        if (up.cl != up.wl) {
            mismatch("unpack: cycle setting cl != wl (not checked)", "cmd " + std::to_string(up.cmd));
            continue;
        }
        const uint32_t base = static_cast<uint32_t>(up.dest) + (up.tops ? (st.xtop & 0x3ff) : 0u);
        for (uint32_t n = 0; n < up.num; ++n) {
            uint32_t e[4], mask;
            unpack_vector(up, n, e, mask);
            ++g_cs.unpack_qw;
            const uint32_t* got = reinterpret_cast<const uint32_t*>(&mem[((base + n) & 0x3ff) << 4]);
            bool ok = true;
            for (int c = 0; c < 4; ++c)
                if (((mask >> c) & 1) != 0 && got[c] != e[c]) ok = false;
            if (!ok) {
                std::snprintf(buf, sizeof buf, "part %u moby %08x class %d unpack cmd %02x dest %03x%s vector %u/%u: %08x %08x %08x %08x vs %08x %08x %08x %08x", k, m.moby, m.class_id, up.cmd, up.dest,
                              up.tops ? "+TOPS" : "", n, up.num, got[0], got[1], got[2], got[3], e[0], e[1], e[2], e[3]);
                mismatch(std::string("unpack: VU1 memory differs from the chain's data (cmd ") + std::to_string(up.cmd) + (up.is_matrix ? ", matrix)" : ")"), buf);
                break;
            }
        }
    }
}

void mrp_scene_check_end_chain() {
    const MrpFrame& f = g_frame;
    if (g_chain_previous || !g_chain_runs) return;
    if (g_k != f.parts.size()) {
        ++g_cs.frames_start_count_diff;
        char buf[128];
        std::snprintf(buf, sizeof buf, "VU1 moby part starts %u, scene parts %zu (%zu mobys)", g_k, f.parts.size(), f.mobys.size());
        mismatch("frame: VU1 start count differs from the scene's part count", buf);
    }
}

void mrp_scene_report() {
    if (!g_check) return;
    const CheckStats& s = g_cs;
    std::printf("[mrp-check] frames %llu: scene mobys %llu parts %llu | VU1 starts %llu (moby part starts %llu, other programs %llu)\n", (unsigned long long)s.frames, (unsigned long long)s.scene_mobys,
                (unsigned long long)s.scene_parts, (unsigned long long)s.starts, (unsigned long long)s.part_starts, (unsigned long long)s.other_starts);
    std::printf("[mrp-check] compared: mscal %llu, projection blocks %llu, matrices %llu, unpacks %llu (%llu vectors), records vs ring %llu (diff %llu), matrix uploads twice %llu (info: differ %llu, of them shadow-mesh mobys %llu), packet counts %llu (diff %llu), moby list entries %llu (flagged visible, not in scene %llu), lights checked %llu (not unit / out of range %llu)\n",
                (unsigned long long)s.mscal_cmp, (unsigned long long)s.proj_cmp, (unsigned long long)s.matrix_cmp, (unsigned long long)s.unpack_cmp, (unsigned long long)s.unpack_qw,
                (unsigned long long)s.record_cmp, (unsigned long long)s.record_ring_diff, (unsigned long long)s.matrix_dup_cmp, (unsigned long long)s.matrix_dup_diff, (unsigned long long)s.matrix_dup_diff_shadow,
                (unsigned long long)s.packet_count_cmp, (unsigned long long)s.packet_count_diff, (unsigned long long)s.list_cmp, (unsigned long long)s.list_flag_not_drawn,
                (unsigned long long)s.light_cmp, (unsigned long long)s.light_odd);
    std::printf("[mrp-check] decode: chains %llu tags %llu overruns %llu open parts %llu unusual VIF %llu guard %llu tags outside RAM %llu | drawn moby with +0x31 == 0: %llu\n", (unsigned long long)g_dec.chains,
                (unsigned long long)g_dec.tags, (unsigned long long)g_dec.overruns, (unsigned long long)g_dec.open_parts, (unsigned long long)g_dec.unusual_vif, (unsigned long long)g_dec.guard, (unsigned long long)g_dec.bad_tags,
                (unsigned long long)s.drawn_flag_zero);
    for (const auto& kv : s.flag_classes)
        std::printf("[mrp-check] info: class %d%s: %llu moby-frames with +0x31 set but not drawn (stale flag: the loop ended early / the shadow-only entry)\n", kv.first % 10000, kv.first >= 10000 ? " (shadow caster)" : "",
                    (unsigned long long)kv.second);
    std::printf("[mrp-check] frames whose chain was not kicked (scene not run: %llu) or was the previous frame's (%llu): not compared\n", (unsigned long long)s.frames_no_chain, (unsigned long long)s.frames_prev_chain);
    if (s.classes.empty()) std::printf("[mrp-check] mismatch classes: none\n");
    for (const auto& kv : s.classes) std::printf("[mrp-check] MISMATCH CLASS x%llu: %s (first: %s)\n", (unsigned long long)kv.second, kv.first.c_str(), s.examples.at(kv.first).c_str());
}

}  // namespace rac1
