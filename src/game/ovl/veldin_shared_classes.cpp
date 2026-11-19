// Veldin's shared classes (formerly shared_classes_tr.cpp, unit veldin_shared):
// the Vendor (class 11, 0x2a57a8), the Infobot (class 750, 0x2d5de8, with helpers 0x2d5988 and 0x2d6cd0),
// and the Thruster-Pack jets (class 167, update 0x2b4198, particle emitter 0x2b3c50, draw callback 0x2b3628).
// Declarations in shared_classes_tr.h.
#include <game/ovl/shared_classes_tr.h>

#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/globals.h>
#include <game/ovl/helmet_tr.h>
#include <game/ovl/level_change.h>
#include <game/ovl/moby_joints.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/shadows.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/vendor_tr.h>

namespace rac1 {

namespace {
int32_t& w32_ref(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }

// Shared hologram draw/spin tail for Vendor states 1 and 2.
void vendor_holo_step(uint32_t m, uint32_t pv) {
    draw_callback_add(OVF(0x002a5040), m);
    quat_from_axis_angle(V(pv + 0x20), 2, lwc1(OV(0x0016139c)));
    quat_from_axis_angle(V(pv + 0x60), 2, lwc1(OV(0x001613a0)));
    swc1(OV(0x0016139c), angle_add(lwc1(OV(0x0016139c)), u2f(0x3c23d70au)));
    swc1(OV(0x001613a0), angle_add(lwc1(OV(0x001613a0)), u2f(0xbca3d70au)));
    const uint32_t holo = lwu(pv + 0xc);
    const float sc = fpu_mul(fpu_mul(lwc1(pv + 0x90), lwc1(lwu(holo + 0x24) + 0x24)), 2.5f);
    swc1(holo + 0x2c, sc);
    moby_update_transform(lwu(pv + 0xc));
}
}  // namespace

// Class 11 (Vendor): pulses glow colour, manages hologram moby (0x477), opens purchase screen on Triangle.
// orig: 0x002a57a8
void vendor_update_tr(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);

    const float phase = fpu_add(lwc1(OV(0x001613a4)), u2f(0x3d4ccccdu));
    swc1(OV(0x001613a4), phase);
    if (u2f(0x4048f5c3u) < phase) {
        swc1(OV(0x001613a4), u2f(0xc048f5c3u));
    }

    const float s = fpu_mul(sin_vu(lwc1(OV(0x001613a4))), 48.0f);
    const int32_t lum = static_cast<int32_t>(f2u(eer::cvt_w(s))) + 0x60;
    sw(m + 0x90, (static_cast<uint32_t>(lum * 0x10101) | 0x80000000u));

    switch (lbu(m + 0x20)) {
    case 0: {
        sb(m + 0x20, 1);
        sw(pv + 0, 0);
        sw(pv + 4, 0);
        const uint32_t holo = moby_spawn(0x477);
        sw(pv + 0xc, holo);
        sh(holo + 0x34, lhu(holo + 0x34) | 2u);
        sh(lwu(pv + 0xc) + 0x32, 0x40);
        swc1(lwu(pv + 0xc) + 0x10, lwc1(m + 0x10));
        swc1(lwu(pv + 0xc) + 0x14, lwc1(m + 0x14));
        swc1(lwu(pv + 0xc) + 0x18, fpu_add(lwc1(m + 0x18), u2f(0x403ccccdu)));
        // 1.02 fix 37 (PAL 2.00 L0:0x2a6b80): guard holo +0x73 = 0x20 on class byte +6 != 0
        const uint32_t h = lwu(pv + 0xc);
        if (!fixes102_vendor_holo_alpha() || lbu(lwu(h + 0x24) + 6) != 0) {
            sb(h + 0x73, 0x20);
        }
        sw(pv + 0x90, 0);
        for (uint32_t i = 0; i < 2; ++i) {
            sub_0024f590(lwu(pv + 0xc), i, pv + 0x10 + i * 0x40);
        }
        moby_update_transform(lwu(pv + 0xc));
        break;
    }
    case 1: {
        if (0.0f < lwc1(pv + 0x90)) {
            swc1(pv + 0x90, fpu_sub(lwc1(pv + 0x90), u2f(0x3dcccccd)));
        }
        if (lwc1(pv + 0x90) <= 0.0f) {
            const uint32_t h = lwu(pv + 0xc);
            sh(h + 0x34, lhu(h + 0x34) | 1u);
        }
        if ((lwu(OV(0x0015f5cc)) & 7u) == 0) {
            if (vec2_distance(V(m + 0x10), V(g::PLAYER + 0x80)) <= 16.0f &&
                abs_f(fpu_sub(lwc1(m + 0x18), lwc1(g::PLAYER + 0x88))) <= 8.0f) {
                sb(m + 0x20, 2);
                if (lbu(m + 0x53) != 1) {
                    moby_anim_set(m, 1, 0, 0xa);
                }
                const uint32_t h = lwu(pv + 0xc);
                sh(h + 0x34, lhu(h + 0x34) & 0xfffeu);
            }
        }
        vendor_holo_step(m, pv);
        break;
    }
    case 2: {
        if (lwc1(pv + 0x90) < 1.0f) {
            swc1(pv + 0x90, fpu_add(lwc1(pv + 0x90), u2f(0x3dcccccd)));
        }
        const float dxy = vec2_distance(V(m + 0x10), V(g::PLAYER + 0x80));
        const float dz = abs_f(fpu_sub(lwc1(m + 0x18), lwc1(g::PLAYER + 0x88)));
        if ((lwu(OV(0x0015f5cc)) & 7u) == 0 && (18.0f < dxy || 10.0f < dz)) {
            sb(m + 0x20, 1);
            if (lbu(m + 0x53) != 0) {
                moby_anim_set(m, 0, 0, 0xa);
            }
            return;
        }
        vendor_holo_step(m, pv);

        bool can_open = true;
        bool can_prompt = true;
        const uint32_t sub = lwu(g::HERO_SUBSTATE);
        if (sub != 0 && lwu(g::HERO_STATE) != 3 && sub != 1) {
            can_open = false;
            can_prompt = false;
        }
        const uint32_t hs = lwu(g::HERO_STATE);
        if (hs == 0x1d || hs == 0x32) {
            can_open = false;
            can_prompt = false;
        }
        if (lbu(g::PLAYER + 0x20a4) != 0) {
            can_open = false;
            can_prompt = false;
        }
        if (lbu(m + 0x52) != 1) {
            can_open = false;
        }
        if (4.0f < dxy || 2.0f < dz) {
            can_open = false;
            can_prompt = false;
        }
        const float bearing = atan2_f(
            fpu_sub(lwc1(m + 0x10), lwc1(g::PLAYER + 0x80)),
            fpu_sub(lwc1(m + 0x14), lwc1(g::PLAYER + 0x84)));
        if (u2f(0x3fc90fdbu) < angle_dist(lwc1(g::PLAYER + 0x98), bearing)) {
            can_open = false;
            can_prompt = false;
        }

        bool msg_ok = false;
        if (can_prompt) {
            msg_ok = hud_message_request(1, TXT(0x53e3)) != 0;
        }
        if ((lwu(0x0013cae4) & 0x10u) != 0 && can_open && msg_ok) {
            vendor_screen_start_tr(m);
            sb(m + 0x20, 3);
        }
        break;
    }
    case 3: {
        const uint32_t h = lwu(pv + 0xc);
        if (h != 0) {
            sw(pv + 0x90, 0);
            const float sc = fpu_mul(fpu_mul(lwc1(lwu(h + 0x24) + 0x24), lwc1(pv + 0x90)), 2.5f);
            swc1(h + 0x2c, sc);
            moby_update_transform(lwu(pv + 0xc));
        }
        break;
    }
    default:
        break;
    }
}

// Advances an Infobot along `path`, banking its pitch/roll and switching to `done_state` at the end.
// orig: 0x002d5988
void infobot_follow_path(uint32_t m, uint32_t path, uint8_t done_state) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    Vec4 target{};
    int32_t seg = 0;
    float frac = 0.0f;
    path_closest_point_v(path, V(pos), target, &seg, &frac, 0, 999.0f, 5.0f, 0.0f);

    const float step_len = lwc1(path + 0x1c);
    const float along = fpu_add(fpu_add(fpu_mul(static_cast<float>(seg), step_len), frac), 2.0f);
    const uint32_t end_pt = path + (lwu(path) << 4);
    const float dist_end = vec3_distance(V(pos), V(end_pt));

    const float dt2 = lwc1(0x0015ed70);
    const float accel = fpu_mul(dt2, 16.0f);
    const float spd = lwc1(pv + 0x60);
    if (dist_end <= fpu_div(fpu_mul(spd, spd), fpu_add(accel, accel))) {
        const float min_spd = fpu_mul(lwc1(OV(0x00161990)), lwc1(0x0015ed6c));
        const float ns = fpu_sub(spd, accel);
        swc1(pv + 0x60, ns < min_spd ? min_spd : ns);
    } else {
        const float max_spd = fpu_mul(lwc1(0x0015ed6c), 16.0f);
        const float ns = fpu_add(spd, accel);
        swc1(pv + 0x60, max_spd < ns ? max_spd : ns);
    }

    seg = float_to_int(fpu_div(along, lwc1(path + 0x1c)));
    const float seg_len = lwc1(path + 0x1c);
    const int32_t npts = lw(path);
    frac = fpu_div(fpu_sub(along, fpu_mul(static_cast<float>(seg), seg_len)), seg_len);
    if (seg >= npts - 1) {
        seg = npts - 2;
        frac = 1.0f;
    }

    const uint32_t p0 = path + (static_cast<uint32_t>(seg) << 4) + 0x10;
    const uint32_t p1 = p0 + 0x10;
    vec3_sub(target, V(p1), V(p0));
    vec3_scale(target, target, frac);
    vec3_add(target, target, V(p0));

    if (vec2_distance(V(path + (lwu(path) << 4)), V(pos)) < 1.0f &&
        lwc1(pv + 0x60) <= fpu_mul(lwc1(OV(0x00161990)), lwc1(0x0015ed6c))) {
        sb(m + 0x20, done_state);
    }

    float aim_x, aim_y;
    if (seg + 1 < lw(path) - 8) {
        const uint32_t look = path + (static_cast<uint32_t>(seg + 1) << 4);
        aim_x = lwc1(look + 0x10);
        aim_y = lwc1(look + 0x14);
    } else {
        aim_x = lwc1(g::PLAYER + 0x80);
        aim_y = lwc1(g::PLAYER + 0x84);
    }
    const float want_yaw = atan2_f(fpu_sub(aim_x, lwc1(m + 0x10)), fpu_sub(aim_y, lwc1(m + 0x14)));
    swc1(m + 0x48, angle_seek_rate(pv + 0x6c, lwc1(m + 0x48), want_yaw, u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3dcccccd)));

    Vec4 move{};
    vec3_sub(move, target, V(pos));
    float move_len = vec3_length(move);
    if (lwc1(pv + 0x60) < move_len) move_len = lwc1(pv + 0x60);
    vec3_set_length(move, move, move_len);
    vec3_add(V(pos), V(pos), move);

    const float rel_yaw = angle_sub(atan2_f(move.x, move.y), lwc1(m + 0x48));
    const float max_spd_p = fpu_mul(lwc1(0x0015ed6c), 16.0f);
    const float pitch_want = fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x60), u2f(0x3eb2b8c2u)), cos_vu(rel_yaw)), max_spd_p);
    const float sin_r = sin_vu(rel_yaw);
    const float dt = lwc1(0x0015ed6c);
    const float dt_sq = lwc1(0x0015ed70);
    const float roll_want = fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x60), u2f(0xbeb2b8c2u)), sin_r), fpu_mul(dt, 16.0f));
    swc1(m + 0x44, angle_seek_rate(pv + 0x68, lwc1(m + 0x44), pitch_want,
                                   fpu_mul(dt_sq, u2f(0x3f060a92u)), fpu_mul(dt_sq, u2f(0x3f860a92u)),
                                   fpu_mul(dt, u2f(0x3f490fdbu))));
    const float dt_sq2 = lwc1(0x0015ed70);
    const float dt2_s = lwc1(0x0015ed6c);
    swc1(m + 0x40, angle_seek_rate(pv + 0x64, lwc1(m + 0x40), roll_want,
                                   fpu_mul(dt_sq2, u2f(0x3f860a92u)), fpu_mul(dt_sq2, u2f(0x40060a92u)),
                                   fpu_mul(dt2_s, u2f(0x3fc90fdbu))));
}

// Emits the four spinning sparkle particles behind an active Infobot.
// orig: 0x002d6cd0
void infobot_glow_particles(uint32_t m) {
    if ((lhu(m + 0x34) & 1u) != 0) return;

    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;

    lq_sq(sp, pos);
    swc1(sp + 8, fpu_add(lwc1(sp + 8), u2f(0x3eaa7efau)));
    vec3_sub(V(sp + 0x10), V(OV(0x00166dc0)), V(pos));
    vec3_set_length(V(sp + 0x10), V(sp + 0x10), u2f(0xbe99999au));
    vec3_set_length(V(sp + 0x20), V(sp + 0x10), u2f(0x3dcccccd));
    vec3_add(V(sp + 0x10), V(sp + 0x10), V(pos));
    swc1(sp + 8, fpu_sub(lwc1(sp + 8), u2f(0x3eaa7efau)));

    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t off = i * 4;
        float ang = fpu_add(lwc1(pv + 0x10 + off), lwc1(pv + 0x20 + off));
        swc1(pv + 0x10 + off, ang);
        if (255.0f <= ang) {
            swc1(pv + 0x10 + off, fpu_sub(ang, 255.0f));
        } else if (ang <= 0.0f) {
            swc1(pv + 0x10 + off, fpu_add(ang, 255.0f));
        }
        if (timer_tick_i32(w32_ref(pv + 0x30 + off)) != 0) {
            sw(pv + 0x30 + off, static_cast<uint32_t>(frame_scale_i(0xff)));
        }
        const int32_t span = frame_scale_i(0xff);
        const float ratio = fpu_div(static_cast<float>(span - lw(pv + 0x30 + off)),
                                    static_cast<float>(frame_scale_i(0xff)));
        const uint32_t col = rgba_lerp(0x4040ffffu, 0x1040ffffu, abs_f(fpu_sub(0.5f, ratio)));
        if (lwu(OV(0x0015f5c4)) != 2) {
            const uint32_t rot = static_cast<uint32_t>(float_to_int(lwc1(pv + 0x10 + off))) & 0xffu;
            spawn_particle_3b(sp + 0x10, col, rot, 0x35, 1, 2, 0, lwc1(pv + 0x40 + off));
        }
        vec3_add(V(sp + 0x10), V(sp + 0x10), V(sp + 0x20));
    }
    ee_stack_free(0x30);
}

// Class 750 (Infobot): 12-state machine (jump table 0x1e9f60) handling idle, path fleeing,
// cage/orbit waiting, collection, cutscene/movie chain, and level-arrival unlock.
// orig: 0x002d5de8
void infobot_update_tr(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x40);
    V(sp) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    const uint32_t pv = lwu(m + 0x78);

    auto settle_pitch_roll = [&](float pitch_want, float roll_want) {
        const float dt_sq = lwc1(0x0015ed70);
        const float dt = lwc1(0x0015ed6c);
        swc1(m + 0x44, angle_seek_rate(pv + 0x68, lwc1(m + 0x44), pitch_want,
                                       fpu_mul(dt_sq, u2f(0x3f060a92u)), fpu_mul(dt_sq, u2f(0x3f860a92u)),
                                       fpu_mul(dt, u2f(0x3f490fdbu))));
        const float dt_sq2 = lwc1(0x0015ed70);
        const float dt2 = lwc1(0x0015ed6c);
        swc1(m + 0x40, angle_seek_rate(pv + 0x64, lwc1(m + 0x40), roll_want,
                                       fpu_mul(dt_sq2, u2f(0x3f860a92u)), fpu_mul(dt_sq2, u2f(0x40060a92u)),
                                       fpu_mul(dt2, u2f(0x3fc90fdbu))));
    };

    auto cache_path_lengths = [](uint32_t path) {
        const int32_t count = lw(path) - 2;
        for (int32_t i = 0; i < count; ++i) {
            const uint32_t a = path + 0x10 + static_cast<uint32_t>(i) * 0x10;
            swc1(a + 0xc, vec3_distance(V(a), V(a + 0x10)));
        }
    };

    // Jump table OV(0x001e9f60), 12 entries (states 0..11)
    bool run_tail = true;
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002d5e5c)
        sb(m + 0x30, 0xff);
        swc1(pv + 0xac, lwc1(m + 0x48));
        lq_sq(pv + 0xc0, m + 0x10);
        if (lwu(pv) != 0) {
            sb(m + 0x20, 2);
            helmet_init_fx(m);
        } else {
            sb(m + 0x20, lwu(pv + 0xb0) != 0 ? 2 : 1);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        }
        if (lw(pv + 0x50) != -1) {
            sb(m + 0x20, 5);
            helmet_init_fx(m);
        } else if (lw(pv + 0x70) != -1) {
            for (int32_t i = 0; i < 8; ++i) {
                const int32_t pid = lw(pv + 0x70 + static_cast<uint32_t>(i) * 4);
                if (pid != -1) cache_path_lengths(lvd::path(pid));
            }
            const uint32_t p0 = lvd::path(lw(pv + 0x70));
            lq_sq(m + 0x10, p0 + 0x10);
            sw(pv + 0xa8, 0);
            sb(m + 0x20, 3);
        }
        break;
    }
    case 1:  // (0x002d6b14)
        break;
    case 2: {  // (0x002d5fa8)
        float radius = 1.0f;
        if (lwu(pv + 0xb0) != 0 && (lhu(m + 0x34) & 1u) != 0) radius = 3.0f;
        if (lw(pv + 4) == 0xc) radius = 3.0f;

        swc1(m + 0x48, angle_seek_rate(pv + 0x6c, lwc1(m + 0x48), lwc1(pv + 0xac),
                                       u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3dcccccd)));
        settle_pitch_roll(0.0f, 0.0f);
        infobot_glow_particles(m);

        if (lbu(0x0013dd40 + static_cast<uint32_t>(lw(pv + 4))) != 0) {
            moby_retire(m);
            run_tail = false;
            break;
        }
        if (vec3_distance(V(g::PLAYER + 0x80), V(m + 0x10)) < radius && lwu(0x001415f8) != 0) {
            sb(m + 0x20, 8);
        }
        break;
    }
    case 3: {  // (0x002d6118)
        infobot_glow_particles(m);
        if (lbu(0x0013dd40 + static_cast<uint32_t>(lw(pv + 4))) != 0) {
            moby_retire(m);
            run_tail = false;
            break;
        }
        if (lw(pv + 0x70 + (lwu(pv + 0xa8) << 2)) == -1) {
            sb(m + 0x20, 2);
            break;
        }
        const float want_yaw = atan2_f(
            fpu_sub(lwc1(g::PLAYER + 0x80), lwc1(m + 0x10)),
            fpu_sub(lwc1(g::PLAYER + 0x84), lwc1(m + 0x14)));
        swc1(m + 0x48, angle_seek_rate(pv + 0x6c, lwc1(m + 0x48), want_yaw,
                                       u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3dcccccd)));
        settle_pitch_roll(0.0f, 0.0f);
        if (vec3_distance(V(g::PLAYER + 0x80), V(m + 0x10)) < 8.0f) {
            sb(m + 0x20, 4);
            talker_word_set(m, 1);
        }
        break;
    }
    case 4: {  // (0x002d6298)
        const int32_t pid = lw(pv + 0x70 + (lwu(pv + 0xa8) << 2));
        infobot_follow_path(m, lvd::path(pid), 3);
        infobot_glow_particles(m);
        if (lbu(m + 0x20) != 4) {
            sw(pv + 0xa8, lwu(pv + 0xa8) + 1u);
        }
        break;
    }
    case 5: {  // (0x002d62f0)
        infobot_glow_particles(m);
        if (lbu(0x0013dd40 + static_cast<uint32_t>(lw(pv + 4))) != 0) {
            moby_retire(m);
            run_tail = false;
            break;
        }
        const float ang = angle_add(lwc1(pv + 0x5c), fpu_mul(lwc1(0x0015ed6c), u2f(0x40490fdbu)));
        swc1(pv + 0x5c, ang);
        swc1(sp + 0, cos_vu(ang));
        swc1(sp + 4, sin_vu(lwc1(pv + 0x5c)));
        swc1(sp + 8, 3.0f);
        swc1(m + 0x48, angle_add(lwc1(pv + 0x5c), u2f(0x3fc90fdbu)));

        const int32_t link_idx = lw(pv + 0x54);
        if (link_idx != -1) {
            const uint32_t linked = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(link_idx) << 8);
            if (lh(linked + 0xa6) == 0x336 && lbu(linked + 0xbc) == 6 && lw(pv + 0x58) != -1) {
                sb(m + 0x20, 6);
                sw(pv + 0x50, 0xffffffffu);
                const uint32_t ep = lvd::path(lw(pv + 0x58));
                lq_sq(m + 0x10, ep + 0x10);
                const uint32_t ep2 = lvd::path(lw(pv + 0x58));
                swc1(m + 0x48, atan2_f(fpu_sub(lwc1(ep2 + 0x20), lwc1(ep2 + 0x10)),
                                       fpu_sub(lwc1(ep2 + 0x24), lwc1(ep2 + 0x14))));
                cache_path_lengths(lvd::path(lw(pv + 0x58)));
            }
        }
        break;
    }
    case 6: {  // (0x002d6468)
        const uint32_t path = lvd::path(lw(pv + 0x58));
        const uint32_t pos = m + 0x10;
        infobot_glow_particles(m);

        int32_t seg = 0;
        float frac = 0.0f;
        path_closest_point_v(path, V(pos), V(sp + 0x20), &seg, &frac, 0, 999.0f, 5.0f, 0.0f);

        const float step_len = lwc1(path + 0x1c);
        const float along = fpu_add(fpu_add(fpu_mul(static_cast<float>(seg), step_len), frac), 2.0f);
        const uint32_t end_pt = path + (lwu(path) << 4);
        const float dist_end = vec3_distance(V(pos), V(end_pt));

        const float dt2 = lwc1(0x0015ed70);
        const float accel = fpu_mul(dt2, 16.0f);
        const float spd = lwc1(pv + 0x60);
        if (dist_end <= fpu_div(fpu_mul(spd, spd), fpu_add(accel, accel))) {
            const float ns = fpu_sub(spd, accel);
            swc1(pv + 0x60, ns < 0.0f ? 0.0f : ns);
        } else {
            const float max_spd = fpu_mul(lwc1(0x0015ed6c), 16.0f);
            const float ns = fpu_add(spd, accel);
            swc1(pv + 0x60, max_spd < ns ? max_spd : ns);
        }

        seg = float_to_int(fpu_div(along, lwc1(path + 0x1c)));
        const float seg_len = lwc1(path + 0x1c);
        frac = fpu_div(fpu_sub(along, fpu_mul(static_cast<float>(seg), seg_len)), seg_len);
        const int32_t npts = lw(path);
        if (seg >= npts - 1) {
            if (lwc1(pv + 0x60) <= fpu_mul(lwc1(0x0015ed70), 16.0f)) {
                sb(m + 0x20, 7);
                lq_sq(pos, path + (static_cast<uint32_t>(npts) << 4));
                break;
            }
            seg = npts - 2;
            frac = 1.0f;
        }

        const uint32_t p0 = path + (static_cast<uint32_t>(seg) << 4) + 0x10;
        const uint32_t p1 = p0 + 0x10;
        vec3_sub(V(sp + 0x20), V(p1), V(p0));
        vec3_scale(V(sp + 0x20), V(sp + 0x20), frac);
        vec3_add(V(sp + 0x20), V(sp + 0x20), V(p0));

        const float want_yaw = atan2_f(fpu_sub(lwc1(p1), lwc1(p0)), fpu_sub(lwc1(p1 + 4), lwc1(p0 + 4)));
        swc1(m + 0x48, angle_seek_rate(pv + 0x6c, lwc1(m + 0x48), want_yaw,
                                       u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3dcccccd)));

        vec3_sub(V(sp + 0x10), V(sp + 0x20), V(pos));
        float move_len = vec3_length(V(sp + 0x10));
        if (lwc1(pv + 0x60) < move_len) move_len = lwc1(pv + 0x60);
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), move_len);
        vec3_add(V(pos), V(pos), V(sp + 0x10));

        const float rel_yaw = angle_sub(atan2_f(lwc1(sp + 0x10), lwc1(sp + 0x14)), lwc1(m + 0x48));
        const float max_spd_p = fpu_mul(lwc1(0x0015ed6c), 16.0f);
        const float pitch_want = fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x60), u2f(0x3eb2b8c2u)), cos_vu(rel_yaw)), max_spd_p);
        const float sin_r = sin_vu(rel_yaw);
        const float dt = lwc1(0x0015ed6c);
        const float roll_want = fpu_div(fpu_mul(fpu_mul(lwc1(pv + 0x60), u2f(0xbeb2b8c2u)), sin_r), fpu_mul(dt, 16.0f));
        settle_pitch_roll(pitch_want, roll_want);
        break;
    }
    case 7: {  // (0x002d6814)
        const uint32_t path = lvd::path(lw(pv + 0x58));
        infobot_glow_particles(m);

        const float r = fpu_add(lwc1(pv + 0xa4), lwc1(0x0015ed6c));
        swc1(pv + 0xa4, 1.0f < r ? 1.0f : r);

        const float ang = angle_add(lwc1(pv + 0x5c), fpu_mul(lwc1(0x0015ed6c), u2f(0x40490fdbu)));
        swc1(pv + 0x5c, ang);
        swc1(sp + 0, fpu_mul(cos_vu(ang), lwc1(pv + 0xa4)));
        swc1(sp + 4, fpu_mul(sin_vu(lwc1(pv + 0x5c)), lwc1(pv + 0xa4)));
        sw(sp + 8, 0);

        const float want_yaw = angle_add(lwc1(pv + 0x5c), u2f(0x3fc90fdbu));
        const float dt = lwc1(0x0015ed6c);
        const float rate = fpu_mul(dt, u2f(0x41c90fdbu));
        swc1(m + 0x48, angle_seek_rate(pv + 0x6c, lwc1(m + 0x48), want_yaw, rate, rate, fpu_mul(dt, u2f(0x41490fdbu))));

        const uint32_t end_pt = path + (lwu(path) << 4);
        vec3_add(V(sp + 0x10), V(sp), V(end_pt));
        lq_sq(m + 0x10, sp + 0x10);

        if (vec3_distance(V(g::PLAYER + 0x80), V(path + (lwu(path) << 4))) < 2.0f && lwu(0x001415f8) != 0) {
            sb(m + 0x20, 8);
        }
        settle_pitch_roll(0.0f, 0.0f);
        break;
    }
    case 8: {  // (0x002d6a10)
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        level_mark_visited(lw(pv + 4));
        checkpoint_mark_collected(lbu(m + 0xb0));
        const int32_t box_id = lw(pv + 8);
        if (box_id != -1) {
            const uint32_t box = lvd::box(lvd::BOXES, box_id);
            checkpoint_save(box + 0x30, box + 0x70);
        }
        sb(m + 0x20, 9);
        const int32_t cs = static_cast<int8_t>(lbu(pv + 0xc));
        if (cs != -1) cutscene_start(cs);
        break;
    }
    case 9: {  // (0x002d6a74)
        if (lwu(OV(0x0015f5c4)) != 2) {
            sb(m + 0x20, 10);
            const int32_t mv = static_cast<int8_t>(lbu(pv + 0xd));
            if (mv != -1) movie_play(mv);
        }
        break;
    }
    case 10: {  // (0x002d6aac)
        if (lwu(OV(0x0015f5c4)) != 2) {
            const int32_t cs = static_cast<int8_t>(lbu(pv + 0xe));
            if (cs != -1) cutscene_start(cs);
            sb(m + 0x20, 11);
        }
        break;
    }
    case 11: {  // (0x002d6ae0)
        if (lwu(OV(0x0015f5c4)) == 2) {
            run_tail = false;
        } else {
            level_change_request(0, -1);
            level_arrival_message(lw(pv + 4));
            moby_retire(m);
            run_tail = false;
        }
        break;
    }
    default:
        break;
    }

    if (run_tail && lw(pv + 0x50) != -1 && lw(pv + 0x54) != -1) {
        const uint32_t linked = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(lw(pv + 0x54)) << 8);
        if (lh(linked + 0xa6) == 0x336) {
            V(sp + 0x10) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
            const uint32_t box_ctr = lvd::box(lvd::BOXES, lw(pv + 0x50)) + 0x30;
            sub_00260400(m, linked, box_ctr, sp + 0x10, box_ctr, sp + 0x10);
            const uint32_t box_ctr2 = lvd::box(lvd::BOXES, lw(pv + 0x50)) + 0x30;
            vec3_add(V(sp + 0x20), V(sp), V(box_ctr2));
            lq_sq(m + 0x10, sp + 0x20);
        }
    }
    ee_stack_free(0x40);
}

// Emits Thruster-Pack jet exhaust particles (types 0x15 and 0x17) along the nozzle trail.
// orig: 0x002b3c50
void class167_emit_particles(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t pv = lwu(m + 0x78);

    const float r = u2f(0x3d4ccccdu);
    swc1(sp + 0, fpu_mul(cos_vu(rand_angle()), r));
    swc1(sp + 4, fpu_mul(sin_vu(rand_angle()), r));
    swc1(sp + 8, -rand_range_f(u2f(0x3c23d70au), u2f(0x3cf5c28fu)));
    mat3_transform(V(sp), V(sp), M(m + 0xc0));

    lq_sq(sp + 0x10, m + 0x10);
    if (lwu(g::HERO_STATE) != 0x81 || ee_rem(lw(OV(0x0015f5cc)), 3) == 0) {
        spawn_particle_15_v(V(sp + 0x10), V(sp), 0x4f007fffu, 0x1fffffffu,
                            static_cast<uint32_t>(frame_scale_i(0xa)), 1, 10000.0f);
    }

    if (lwu(pv + 0xc) != 0) {
        const float half_d = fpu_mul(vec3_distance(V(pv + 0x10), V(sp + 0x10)), 0.5f);
        vec3_sub(V(sp + 0x20), V(pv + 0x10), V(sp + 0x10));
        for (int32_t i = 1; i < 2; ++i) {
            vec3_set_length(V(sp + 0x30), V(sp + 0x20), fpu_mul(half_d, static_cast<float>(i)));
            vec3_add(V(sp + 0x30), V(sp + 0x10), V(sp + 0x30));
            spawn_particle_15_v(V(sp + 0x30), V(sp), 0x4f007fffu, 0x1fffffffu,
                                static_cast<uint32_t>(frame_scale_i(0xa)), 1, 10000.0f);
        }
    }

    sw(pv + 0, static_cast<uint32_t>(rand_mod(frame_scale_i(0xa))));
    uint32_t col = 0x00808080u;
    float growth = u2f(0x3f89999au);

    V(sp) = Vec4{0.0f, 0.0f, u2f(0xbe99999au), 1.0f};
    mat3_transform(V(sp), V(sp), M(m + 0xc0));
    vec3_add(V(sp + 0x10), V(m + 0x10), V(sp));

    V(sp) = Vec4{0.0f, 0.0f, u2f(0xbd99999au), 1.0f};
    mat3_transform(V(sp), V(sp), M(m + 0xc0));

    const float z_base = u2f(0x3ccccccdu);
    swc1(sp + 8, z_base);
    if (lwu(g::HERO_STATE) == 8) {
        col = 0x00a0a0a0u;
        growth = u2f(0x3f821cacu);
        swc1(sp + 8, fpu_sub(z_base, fpu_mul(lwc1(0x0015ed6c), 5.0f)));
    }
    if (lwu(g::HERO_STATE) == 0x81) {
        col = 0x00a0a0a0u;
        growth = u2f(0x3f84bc6au);
        swc1(sp + 8, fpu_sub(lwc1(sp + 8), fpu_mul(lwc1(0x0015ed6c), 3.0f)));
    }

    const uint32_t p0 = spawn_particle_17(sp + 0x10, 6, sp, col, u2f(0x3d4ccccdu), u2f(0x3f8147aeu), growth, 30000.0f);
    if (p0 != 0) sb(p0 + 3, 0x44);

    if (lwu(pv + 0xc) != 0) {
        const float step_d = fpu_mul(vec3_distance(V(pv + 0x10), V(sp + 0x10)), u2f(0x3e2aab8au));
        vec3_sub(V(sp + 0x20), V(pv + 0x10), V(sp + 0x10));
        int32_t steps = 4;
        const uint32_t hs = lwu(g::HERO_STATE);
        if (hs == 8) steps = 2;
        if (hs == 0x81) steps = 1;
        for (int32_t i = 1; i < steps; ++i) {
            vec3_set_length(V(sp + 0x30), V(sp + 0x20), fpu_mul(step_d, static_cast<float>(i)));
            vec3_add(V(sp + 0x30), V(sp + 0x10), V(sp + 0x30));
            const uint32_t pt = spawn_particle_17(sp + 0x30, 6, sp, col, u2f(0x3d4ccccdu), u2f(0x3f8147aeu), growth, 30000.0f);
            if (pt != 0) sb(pt + 3, 0x44);
        }
    }

    lq_sq(pv + 0x10, sp + 0x10);
    sw(pv + 0xc, 1);
    ee_stack_free(0x40);
}

// Class 167 (Thruster-Pack jet): 5-state machine (jump table 0x1e9a20) controlling jet ramp-up,
// active burn, particle emission, and ramp-down.
// orig: 0x002b4198
void class167_update_tr(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    bool thrust = false;
    const uint8_t state = lbu(m + 0x20);

    if (lwu(0x001404f8) == 3 && (state == 1 || state == 3)) {
        const uint32_t hs = lwu(g::HERO_STATE);
        if (hs - 0xbu < 4u) {
            thrust = (lh(g::PLAYER + 0x41e) == 0);
        } else if (hs == 0x1c) {
            const uint32_t hm = lwu(g::HERO_MOBY);
            if (lbu(hm + 0x52) == lbu(hm + 0x53)) {
                const float f = lwc1(g::PLAYER + 0xaa8);
                if (6.0f <= f && f <= 16.0f) {
                    thrust = (lh(g::PLAYER + 0x41e) == 0);
                }
            }
        }
        if (lwu(g::HERO_STATE) == 0x10 && lw(g::PLAYER + 0x198) < frame_scale_i(0x2c)) {
            thrust = true;
        }
        if (lwu(g::HERO_STATE) == 0x22) {
            if (lw(g::PLAYER + 0x198) < frame_scale_i(0xf)) {
                thrust = true;
            } else if (frame_scale_i(0x21) < lw(g::PLAYER + 0x198) && lh(g::PLAYER + 0x30e) != 0) {
                thrust = true;
            }
        }
        const uint32_t hs2 = lwu(g::HERO_STATE);
        if (hs2 == 8 || hs2 == 0x81) {
            thrust = true;
        }
    }

    auto set_rates = [&](int32_t frames) {
        sw(pv + 0, static_cast<uint32_t>(frame_scale_i(frames)));
        swc1(pv + 0x30, fpu_div(u2f(0x3c4ccccdu), static_cast<float>(frame_scale_i(frames))));
        swc1(pv + 0x34, fpu_div(u2f(0x3e4ccccdu), static_cast<float>(frame_scale_i(frames))));
        swc1(pv + 0x38, fpu_div(u2f(0x3d4ccccdu), static_cast<float>(frame_scale_i(frames))));
        swc1(pv + 0x3c, fpu_div(u2f(0x3ccccccdu), static_cast<float>(frame_scale_i(frames))));
    };

    // Jump table OV(0x001e9a20), 5 entries (states 0..4)
    switch (state) {
    case 0:  // (0x002b4330)
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.5f));
        break;
    case 1:  // (0x002b4364)
        if (sub_0020d498(3) != 3) {
            moby_retire(m);
            return;
        }
        if (thrust) {
            sb(m + 0x20, 2);
            set_rates(2);
            sw(pv + 0x20, 0);
            sw(pv + 0x24, 0);
            sw(pv + 0x28, 0);
            sw(pv + 0x2c, 0);
        }
        break;
    case 2:  // (0x002b4454)
        swc1(pv + 0x20, fpu_add(lwc1(pv + 0x20), lwc1(pv + 0x30)));
        swc1(pv + 0x24, fpu_add(lwc1(pv + 0x24), lwc1(pv + 0x34)));
        swc1(pv + 0x28, fpu_add(lwc1(pv + 0x28), lwc1(pv + 0x38)));
        swc1(pv + 0x2c, fpu_add(lwc1(pv + 0x2c), lwc1(pv + 0x3c)));
        sw(pv + 8, lwu(g::PLAYER + 0x1180));
        draw_callback_add(OVF(0x002b3628), m);
        if (timer_tick_i32(w32_ref(pv)) != 0) {
            sb(m + 0x20, 3);
            swc1(pv + 0x20, u2f(0x3c4ccccdu));
            swc1(pv + 0x24, u2f(0x3e4ccccdu));
            swc1(pv + 0x28, u2f(0x3d4ccccdu));
            swc1(pv + 0x2c, u2f(0x3ccccccdu));
            sw(pv + 0xc, 0);
        }
        break;
    case 3:  // (0x002b450c)
        sw(pv + 8, lwu(g::PLAYER + 0x1180));
        draw_callback_add(OVF(0x002b3628), m);
        if (thrust) {
            swc1(pv + 0x20, u2f(0x3c4ccccdu));
            class167_emit_particles(m);
        } else {
            sb(m + 0x20, 4);
            set_rates(8);
        }
        break;
    case 4:  // (0x002b4608)
        if (timer_tick_i32(w32_ref(pv)) != 0) {
            sb(m + 0x20, 1);
            break;
        }
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), lwc1(pv + 0x30)));
        swc1(pv + 0x24, fpu_sub(lwc1(pv + 0x24), lwc1(pv + 0x34)));
        swc1(pv + 0x28, fpu_sub(lwc1(pv + 0x28), lwc1(pv + 0x38)));
        swc1(pv + 0x2c, fpu_sub(lwc1(pv + 0x2c), lwc1(pv + 0x3c)));
        sw(pv + 8, lwu(g::PLAYER + 0x1180));
        draw_callback_add(OVF(0x002b3628), m);
        break;
    default:
        break;
    }
}

// Class 167 draw callback: aligns the nozzle basis to Clank's jet joints, draws two flame quads,
// a camera-facing nozzle glow quad, and the VU1 flame-cone mesh (196 vertices, two prim lists).
// orig: 0x002b3628
void class167_draw_tr(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pack = lwu(pv + 8);
    if (pack == 0 || lh(pack + 0xa6) != 0x260) return;

    const uint32_t sp = ee_stack_alloc(0x1e0);
    const uint32_t d = sp;             // 0x00..0x8f quad descriptor
    const uint32_t basis = sp + 0x90;  // 0x90..0xcf nozzle basis + pos
    const uint32_t jpos = sp + 0xd0;   // 0xd0..0xef two joint positions
    const uint32_t jids = sp + 0xf0;   // 0xf0..0xf7 two joint ids
    const uint32_t up = sp + 0x100;
    const uint32_t m_quad = sp + 0x110;
    const uint32_t m_off = sp + 0x150;
    const uint32_t cam_m = sp + 0x190;
    const uint32_t pt = sp + 0x1d0;

    sd(d + 0x88, (0x8000ull << 24) | 0x48ull);
    sd(d + 0x80, (0xff90ull << 32) | 0x260ull);
    sd(d + 0x70, 0);

    const uint32_t id_src = lwu(pv + 4) != 0 ? OV(0x00161488) : OV(0x00161490);
    for (uint32_t i = 0; i < 8; ++i) sb(jids + i, lbu(id_src + i));
    moby_joint_positions(pack, 2, jids, jpos);

    V(up) = Vec4{0.0f, 1.0f, 0.0f, 0.0f};
    vec3_sub(V(basis + 0x20), V(jpos), V(jpos + 0x10));
    vec3_set_length(V(basis + 0x20), V(basis + 0x20), 1.0f);
    sw(basis + 0x2c, 0);
    vec3_cross_ba(V(basis + 0x00), V(up), V(basis + 0x20));
    vec3_set_length(V(basis + 0x00), V(basis + 0x00), 1.0f);
    sw(basis + 0x0c, 0);
    vec3_cross_ba(V(basis + 0x10), V(basis + 0x20), V(basis + 0x00));
    sw(basis + 0x1c, 0);
    lq_sq(basis + 0x30, jpos + 0x10);
    swc1(basis + 0x3c, 1.0f);

    lq_sq(m + 0xc0, basis + 0x00);
    lq_sq(m + 0xd0, basis + 0x10);
    lq_sq(m + 0xe0, basis + 0x20);
    lq_sq(m + 0x10, basis + 0x30);

    mat4_identity(M(m_off));
    sw(m_off + 0x30, 0);
    sw(m_off + 0x34, 0);
    swc1(m_off + 0x38, u2f(0xbd99999au));
    swc1(m_off + 0x3c, 1.0f);
    mat4_mul(M(m_quad), M(basis), M(m_off));

    sd(d + 0x78, effect_tex0(0xc));
    const uint32_t verts0 = OV(0x001da9e0);
    const uint32_t uvs0 = OV(0x001daa60);
    for (int32_t i = 0; i < 4; ++i) {
        sw(d + 0x40 + static_cast<uint32_t>(i) * 4, 0x80808080u);
        const uint32_t dst = d + static_cast<uint32_t>(i) * 0x10;
        const uint32_t src = verts0 + static_cast<uint32_t>(i) * 0x10;
        vec3_scale(V(dst), V(src), lwc1(pv + 0x2c));
        if (i != 0 && i != 2) {
            swc1(dst + 8, fpu_sub(lwc1(dst + 8), rand_range_f(0.0f, u2f(0x3dcccccd))));
        }
        swc1(d + 0x50 + static_cast<uint32_t>(i) * 8, lwc1(uvs0 + static_cast<uint32_t>(i) * 8));
        swc1(d + 0x54 + static_cast<uint32_t>(i) * 8, lwc1(uvs0 + 4 + static_cast<uint32_t>(i) * 8));
    }
    quad_draw(d, m_quad);

    const uint32_t verts1 = OV(0x001daa20);
    for (int32_t i = 0; i < 4; ++i) {
        const uint32_t dst = d + static_cast<uint32_t>(i) * 0x10;
        const uint32_t src = verts1 + static_cast<uint32_t>(i) * 0x10;
        vec3_scale(V(dst), V(src), lwc1(pv + 0x2c));
        if (i != 0 && i != 2) {
            swc1(dst + 8, fpu_sub(lwc1(dst + 8), rand_range_f(0.0f, u2f(0x3dcccccd))));
        }
    }
    quad_draw(d, m_quad);

    sd(d + 0x78, effect_tex0(0xb));
    vec3_scale(V(cam_m + 0x30), V(basis + 0x20), u2f(0xbe051eb8u));
    vec3_add(V(cam_m + 0x30), V(cam_m + 0x30), V(basis + 0x30));
    vec3_sub(V(cam_m + 0x00), V(OV(0x00166dc0)), V(cam_m + 0x30));
    vec3_set_length(V(cam_m + 0x00), V(cam_m + 0x00), 1.0f);
    vec3_cross_ba(V(cam_m + 0x10), V(cam_m + 0x00), V(0x0013f5e0));
    vec3_set_length(V(cam_m + 0x10), V(cam_m + 0x10), -1.0f);
    vec3_cross_ba(V(cam_m + 0x20), V(cam_m + 0x10), V(cam_m + 0x00));

    for (uint32_t i = 0; i < 4; ++i) {
        sw(d + 0x40 + i * 4, 0x7f2020ffu);
        const uint32_t dst = d + i * 0x10;
        vec3_scale(V(dst), V(verts0 + i * 0x10), u2f(0x3dcccccd));
        mat4_transform(V(dst), V(dst), M(cam_m));
    }
    quad_draw(d, 0);

    sub_002a0ba8(0x6, effect_tex0(0xa));
    sub_002a0ba8(0x42, ld(d + 0x88));
    sub_002a0ba8(0x8, 0);
    sub_002a0ba8(0x14, (0xff90ull << 32) | 0x260ull);
    sub_002a0ba8(0x4a, 0);
    shadows_vu1_setup();

    const uint32_t src_v = OV(0x001d9470);
    const uint32_t dst_v = OV(0x001daa80);
    const uint32_t st_v = OV(0x001da0b4);
    for (uint32_t i = 0; i < 0xc4; ++i) {
        vec3_scale(V(pt), V(src_v + i * 0x10), lwc1(pv + 0x20));
        mat4_transform(V(pt), V(pt), M(basis));
        swc1(dst_v + i * 0xc + 0, lwc1(pt + 0));
        swc1(dst_v + i * 0xc + 4, lwc1(pt + 4));
        swc1(dst_v + i * 0xc + 8, lwc1(pt + 8));
        const float nv = fpu_sub(lwc1(st_v + i * 8), u2f(0x3ccccccdu));
        swc1(st_v + i * 8, nv < -7.0f ? fpu_add(nv, 7.0f) : nv);
    }

    prim_list_packet(0x94, OV(0x001daa80), OV(0x001da6d0), OV(0x001da0b0), 1);
    prim_list_packet(0x32, OV(0x001db158), OV(0x001da918), OV(0x001da540), 1);
    ee_stack_free(0x1e0);
}

}  // namespace rac1
