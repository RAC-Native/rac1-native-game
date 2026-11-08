// Ported overlay code: the game's sounds. 30 slots at 0x13e5c0 (stride 0x70): +0 the driver's
// handle, +4 the state (7 to start, 1 started, 2 playing, 4 to stop, 6 stopping, 0 free), +8 the
// sound's definition (+0x1a the sound, +0x1c the bank), +0x18 the moby it follows, +0x20 the
// position; their per-frame update 0x28cc08 sends them to 989snd.
#include <game/lib/snd989.h>
#include <runtime/iop/snd989_iop.h>
#include <game/ovl/aridia_classes_tr.h>
#include <game/ovl/blarg_call.h>
#include <game/ovl/umbris_call.h>
#include <game/ovl/umbris_hooks.h>
#include <game/ovl/gaspar_hooks.h>
#include <game/ovl/batalia_hooks.h>
#include <game/ovl/orxon_hooks.h>
#include <game/ovl/pokitaru_hooks.h>
#include <game/ovl/hoven_hooks.h>
#include <game/ovl/gemlik_hooks.h>
#include <game/ovl/oltanis_hooks.h>
#include <game/ovl/quartu_hooks.h>
#include <game/ovl/kalebo_hooks.h>
#include <game/ovl/veldin_orbit_hooks.h>
#include <game/ovl/veldin_finale_hooks.h>
#include <game/ovl/kerwan_hooks.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/ovl_reloc.h>
#include <game/boot/level_read.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/rilgar_hooks.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/novalis_level_fns.h>
#include <game/ovl/ovl.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

int advanced_audio_channels();  // advanced.cpp: Options > Sound's 5.1 / 7.1 (the device's channels)

// A play reply: the slot's handle; none (0) frees it, a started one is playing.
// orig: 0x0028e108
void snd_play_reply(int32_t reply, uint64_t data) {
    const uint32_t s = static_cast<uint32_t>(data);
    if (s == 0) return;
    sw(s, static_cast<uint32_t>(reply));
    if (reply == 0) {
        sb(s + 4, 0);
        sw(s + 0x18, 0);
        sw(s + 0x1c, 0);
    } else if (lbu(s + 4) == 1) {
        sb(s + 4, 2);
    }
}

// A set / still-playing reply: the slot's handle; none (0) means the sound ended: freed.
// orig: 0x0028e150
void snd_set_reply(int32_t reply, uint64_t data) {
    const uint32_t s = static_cast<uint32_t>(data);
    if (s == 0) return;
    sw(s, static_cast<uint32_t>(reply));
    if (reply == 0) {
        sb(s + 4, 0);
        sw(s + 0x18, 0);
        sw(s + 0x1c, 0);
    }
}

// A point near the camera the sounds are heard from: the camera plus a random offset (0.5..6),
// pulled back to 3/4 of the way to a wall in between (collision 0x82, ignoring 0x13e5bc).
// orig: 0x0028c760
void sound_listener_point_v(Vec4& p) {
    rand_dir_scaled_v(p, 0.5f, 6.0f);
    vec3_add(p, p, V(OV(0x00166dc0)));
    if (segment_collide_v(V(OV(0x00166dc0)), p, 0x82, lwu(0x0013e5bc), nullptr) != 0) {
        vec3_sub(p, V(OV(0x00173e60)), V(OV(0x00166dc0)));
        vec3_scale(p, p, 0.75f);
        vec3_add(p, p, V(OV(0x00166dc0)));
    }
}
// orig: 0x0028c760 (EE address)
void sound_listener_point(uint32_t p) { sound_listener_point_v(V(p)); }

// Whether a sound is blocked from a listener point: the line from `from` to a point 3/4 of the
// way from the camera to the sound (at most 64 out), collision 0x82 ignoring its moby.
// orig: 0x0028c810
uint32_t sound_line_blocked_v(uint32_t slot, const Vec4& from) {
    Vec4 t = Vec4{};
    vec3_sub(t, V(slot + 0x20), V(OV(0x00166dc0)));
    vec3_scale(t, t, 0.75f);
    vec3_clamp_length_vu(t, t, 64.0f);
    vec3_add(t, t, V(OV(0x00166dc0)));
    return static_cast<uint32_t>(segment_collide_v(from, t, 0x82, lwu(slot + 0x18), nullptr));
}

// A sound's pan from its position: its angle in the camera's view, in degrees, scaled down
// within 1 unit of the camera (the view matrix `m`, transposed).
// orig: 0x0028c9e8
int32_t sound_pan(uint32_t slot, uint32_t pos, const Mat4& m) {
    (void)slot;
    Vec4 t = Vec4{};
    vec3_sub(t, V(pos), V(OV(0x00166dc0)));
    mat4_transform(t, t, m);
    const float k = clampf(vec2_length(t) - 1.0f, 0.0f, 1.0f);
    const float a = atan2_f(t.x, t.y);
    return float_to_int(-a * 180.0f * k * 0.31830987f);
}

// The sounds' frame: the reverb and the group volumes, the listener's motion (the camera's
// last 4 positions, 0x13e550), each slot's volume (distance 0x28c9a0, occlusion: 36 bytes of
// blocked lines, 0x28c810), pan and Doppler pitch, then the starts / updates / stops sent to
// 989snd; the music streams (0x265aa8); the batch released, sent and held again.
// orig: 0x0028cc08
int32_t sound_frame() {
    const int32_t under = lw(OV(0x00167014));  // the hero under water: muffled
    float surface = 0.0f;
    if (under != 0) surface = lwc1(0x0013f640);
    snd_flush();
    const uint32_t rv = lbu(0x0013e5bb);
    if ((rv & 8) != 0) {
        snd_cmd_ef68(2, 0, 0, 0, 0);
    } else if ((rv & 0x13) != 0) {
        snd_cmd_ef68(2, lbu(0x0013e5b8), lwu(0x0013e5b4), lbu(0x0013e5b9), lbu(0x0013e5ba));
    } else if ((rv & 4) != 0) {
        snd_cmd_efe0(2, lwu(0x0013e5b4), 0xc, 3);
    }
    sb(0x0013e5bb, 0);
    Vec4 listener = Vec4{};
    if ((lw(OV(0x0015f5c4)) == 0 || lw(OV(0x0015f5c4)) == 2) && lw(OV(0x0015f5a8)) == 0) {
        sound_listener_point_v(listener);
    }
    Vec4 motion = Vec4{};
    const int32_t cur = (lw(0x0013e590) + 1) % 4;
    sw(0x0013e590, static_cast<uint32_t>(cur));
    V(0x0013e550 + static_cast<uint32_t>(cur) * 0x10) = V(OV(0x00166dc0));
    int32_t n = 0;
    int32_t a = cur;
    int32_t b = (cur + 3) % 4;
    if (b != a) {
        do {
            Vec4 d = Vec4{};
            vec3_sub(d, V(0x0013e550 + static_cast<uint32_t>(a) * 0x10), V(0x0013e550 + static_cast<uint32_t>(b) * 0x10));
            if (!(vec3_length(d) < lwc1(0x0015ed6c) * 60.0f)) break;
            ++n;
            vec3_add(motion, motion, d);
            a = b;
            b = (b + 3) % 4;
        } while (b != lw(0x0013e590));
    }
    if (n >= 2) vec3_scale(motion, motion, fpu_div(1.0f, int_to_float(n)));
    if (lw(OV(0x0015f5c4)) == 2) {
        snd_set_master_volume(1, 0);
        snd_set_master_volume(0, lw(0x0013e598) / 2);
        snd_set_master_volume(3, lw(0x0013e5a4) / 2);
    } else {
        int32_t v = lw(0x0013e59c);
        if (under != 0) v = v * 3 / 5;
        snd_set_master_volume(1, v);
        snd_set_master_volume(0, lw(0x0013e598));
        snd_set_master_volume(3, lw(0x0013e5a4));
    }
    snd_set_master_volume(2, lw(0x0013e5a0));
    snd_set_master_volume(4, lw(0x0013e5a8));
    snd_set_master_volume(5, lw(0x0013e5ac));
    uint32_t todo[30] = {};
    int32_t vol[30] = {};
    float dop[30] = {};
    const int32_t mode = lw(OV(0x0015f5c4));
    for (uint32_t i = 0; i < 30; ++i) {
        const uint32_t e = 0x0013e5c0 + i * 0x70;
        if (lbu(e + 4) != 7) {
            const uint32_t h = lwu(e);
            if (h == 0 || h == 0xffffffffu) continue;
        }
        const uint32_t m = lwu(e + 0x18);
        bool dead = false;
        if (m != 0 && (lbu(m + 0x20) == 0xfe || lbu(m + 0x20) == 0xfd)) dead = true;
        if (dead) sw(e + 0x18, 0);
        const uint32_t st = lbu(e + 4);
        if (st == 4 || (st != 6 && dead && lbu(lwu(e + 8) + 0x18) != 0)) {
            todo[i] = 0x20;
            continue;
        }
        if (!(mode == 0 || mode == 2 || mode == 6) && st != 7) {
            todo[i] = 0x10;
            continue;
        }
        Vec4 d = Vec4{};
        const uint32_t mo = lwu(e + 0x18);
        if (mo != 0 && (lbu(e + 5) & 8) == 0) {
            if ((lbu(e + 5) & 0x40) != 0) {
                Vec4 p = Vec4{};
                mat3_transform(p, V(e + 0x30), M(mo + 0xc0));
                vec3_add(p, p, V(mo + 0x10));
                vec3_sub(d, p, V(e + 0x20));
                V(e + 0x20) = p;
            } else {
                swc1(e + 0x28, lwc1(e + 0x28) - 1.0f);
                vec3_sub(d, V(mo + 0x10), V(e + 0x20));
                V(e + 0x20) = V(lwu(e + 0x18) + 0x10);
                swc1(e + 0x28, lwc1(e + 0x28) + 1.0f);
            }
        }
        vec3_sub(d, d, motion);
        Vec4 to = Vec4{};
        vec3_sub(to, V(OV(0x00166dc0)), V(e + 0x20));
        vec3_set_length(to, to, 1.0f);
        dop[i] = vec3_dot(to, d);
        int32_t f;
        if ((lbu(e + 5) & 0x10) == 0) {
            f = sub_0028c9a0(e, e + 0x20);
            const int32_t v = f * lw(e + 0x10);
            vol[i] = (v < 0 ? v + 0x3ff : v) >> 10;
            if ((lbu(lwu(e + 8) + 0x19) & 2) == 0) todo[i] |= 8;
        } else {
            f = 0x400;
            vol[i] = lw(e + 0x10);
        }
        if (under != 0 && (lbu(lwu(e + 8) + 0x19) & 4) == 0 && surface < lwc1(e + 0x28)) vol[i] /= 2;
        todo[i] |= 1;
        if (f < 0x20 && vol[i] < 0x20 && (lbu(e + 5) & 4) != 0) {
            todo[i] = 0x20;
            continue;
        }
        if ((lbu(e + 5) & 1) == 0) {
            const uint32_t t = todo[i];
            todo[i] = t | 2;
            if ((lbu(e + 5) & 0x20) == 0) todo[i] = t | 6;
        }
    }
    for (uint32_t i = 0; i < 30; ++i) {
        if ((todo[i] & 8) == 0) continue;
        const uint32_t e = 0x0013e5c0 + i * 0x70;
        int32_t blocked;
        if (lbu(e + 4) == 7) {
            if (!(mode == 0 || mode == 2) || lw(OV(0x0015f5a8)) != 0) continue;
            if (lwu(OV(0x0015f5cc)) != lwu(0x0013f2e8)) {
                for (uint32_t k = 0; k < 6; ++k) sound_listener_point(0x0013f2f0 + k * 0x10);
                sw(0x0013f2e8, lwu(OV(0x0015f5cc)));
            }
            blocked = 0;
            for (uint32_t k = 0, j = 0; j < 0x24; ++k, j += 6) {
                const uint32_t o = sound_line_blocked_v(e, V(0x0013f2f0 + k * 0x10));
                sb(e + 0x44 + j, o);
                for (uint32_t c = 1; c < 6; ++c) sb(e + 0x44 + j + c, lbu(e + 0x44 + j));
                if (lbu(e + 0x44 + j) != 0) blocked += 6;
            }
            if (blocked >= 0x24) {
                vol[i] = 0;
                continue;
            }
            if (blocked < 0x13) continue;
        } else {
            if ((mode == 0 || mode == 2) && lw(OV(0x0015f5a8)) == 0) {
                const uint32_t k = (lwu(e + 0x40) + 1) % 0x24;
                sw(e + 0x40, k);
                const uint32_t fc = lwu(OV(0x0015f5cc));
                uint32_t now = (i ^ fc) & 1;
                if ((lbu(e + 5) & 4) == 0) now = ((fc ^ i) & 3) == 0 ? 1 : 0;
                if (now != 0) {
                    sb(e + 0x44 + lwu(e + 0x40), sound_line_blocked_v(e, listener));
                } else {
                    sb(e + 0x44 + k, lbu(e + 0x44 + (k + 0x23) % 0x24));
                }
            }
            blocked = bytes_sum(e + 0x44, 0x24);
            if (blocked >= 0x24) {
                vol[i] = 0;
                continue;
            }
            if (blocked < 0x13) continue;
        }
        vol[i] = (0x24 - blocked) * vol[i] / 0x12;
    }
    Mat4 view = Mat4{};
    mat3_transpose(view, M(OV(0x00166fd0)));
    const bool surround = advanced_audio_channels() > 2;  // (host: the places below, 5.1 / 7.1 only)
    for (uint32_t i = 0; i < 30; ++i) {
        const uint32_t t = todo[i];
        if (t == 0) continue;
        const uint32_t e = 0x0013e5c0 + i * 0x70;
        const uint32_t h = lwu(e);
        sw(e, 0xffffffffu);
        if ((t & 0x20) != 0) {
            if (lbu(e + 4) == 7) {
                sb(e + 4, 0);
                sw(e + 0x18, 0);
                sw(e + 0x1c, 0);
            } else {
                snd_stop_sound(h);
                sb(e + 4, 6);
                snd_sound_is_playing(h, OVF(0x0028e150), e);
            }
            continue;
        }
        if ((t & 0x10) != 0) {
            snd_sound_is_playing(h, OVF(0x0028e150), e);
            continue;
        }
        const int32_t pb = lw(e + 0x14);
        uint32_t flags = pb != 0 ? 0x11 : 1;
        int32_t pan = 0, pm = 0;
        if ((t & 2) != 0) {
            pan = sound_pan(e, e + 0x20, view);
            flags |= 6;
        }
        if ((todo[i] & 4) != 0) {
            flags |= 8;
            pm = snd_pitch_from_cents(float_to_int(dop[i] * 300.0f));
        }
        if (under != 0 && (lbu(lwu(e + 8) + 0x19) & 8) == 0) {
            flags |= 8;
            pm -= 0x5f4;
        }
        // Host (5.1 / 7.1): the places the game's pan does not give, outside the commands (the stereo mix keeps pan 0;
        // sound_pan only reads; runtime/iop/snd989_iop.h):
        // - a world sound played without a pan (flags bit 0) but attenuated by its distance (0x10 clear), not on the
        //   hero (Kalebo III's class 471, Rilgar's 855, Kerwan's 574, Quartu / Drek's fleet 655): placed by its direction;
        // - an ambience bed (a level emitter's type 4 / 5, flags 0x15: its slot moved to the camera each frame): diffuse.
        // The menus' and HUD's sounds and the help jingle (0x11), Ratchet's own and Giant Clank's (0x31) stay on the front.
        int place = 0, place_pan = 0;
        if (surround && (t & 2) == 0) {
            const uint32_t f = lbu(e + 5), mob = lwu(e + 0x18);
            const bool hero = mob != 0 && (mob == lwu(g::PLAYER + 0x2080) || mob == lwu(g::PLAYER + 0x1090) || mob == lwu(g::PLAYER + 0xa88));
            if ((f & 0x11) == 1 && !hero) {
                place = 1;
                place_pan = sound_pan(e, e + 0x20, view);
            } else if (f == 0x15 && lwu(e + 0x1c) != 0) {
                place = 2;
            }
        }
        if (lbu(e + 4) == 7) {
            sb(e + 4, 1);
            const uint32_t def = lwu(e + 8);
            if (place != 0) snd989_host_place_next(place, place_pan);
            snd_play_sound(lwu(def + 0x1c), static_cast<uint32_t>(static_cast<int32_t>(lh(def + 0x1a))), vol[i], pan, pm, pb, OVF(0x0028e108), e);
        } else {
            snd_set_sound(h, flags, vol[i], pan, pm, pb, OVF(0x0028e150), e);
            if (place != 0) snd989_host_place(h, place, place_pan);
        }
    }
    music_streams_frame();
    snd_release();
    snd_flush();
    snd_hold();
    sw(0x0013e5bc, 0);
    return 0;
}

// The level's sound `idx` (table 0x15f5f4, 0x20 bytes each; none past its count 0x15f574) started
// with `flags` on `moby`: its slot keeps the index (+0xe) and the moby (+0x18); the slot or -1.
// orig: 0x0028dd40
int32_t sound_play_indexed(int32_t idx, uint32_t flags, uint32_t moby) {
    if (!(idx < lw(OV(0x0015f574)))) return -1;
    const int32_t s = sound_start(lwu(OV(0x0015f5f4)) + static_cast<uint32_t>(idx) * 0x20, flags, moby, 0, 0x400);
    if (!(s < 0)) {
        const uint32_t e = static_cast<uint32_t>(s) * 0x70 + 0x0013e550;
        sh(e + 0x7e, static_cast<uint32_t>(idx));
        sw(e + 0x88, moby);
    }
    return s;
}

// A level sound (the level's sound list 0x15f5f4, count 0x15f5f0) played at an emitter's
// position (+0x40); the slot remembers the sound and the emitter. -1 when none.
// orig: 0x0028ddd8
int32_t level_sound_play(int32_t idx, uint32_t flags, uint32_t emitter, int32_t volume) {
    if (on_umbris()) return static_cast<int32_t>(umb_call(umb_level_sound_play_u, static_cast<uint32_t>(idx), flags, emitter, static_cast<uint32_t>(volume)).v0);  // (level 7's own: L7:0x002b4a00)
    if (on_blarg()) return static_cast<int32_t>(blg_call(blg_level_sound_play_u, static_cast<uint32_t>(idx), flags, emitter, static_cast<uint32_t>(volume)).v0);  // (level 6's own: L6:0x0029aec0)
    if (!(idx < lw(OV(0x0015f5f0)))) return -1;
    const int32_t ch = sound_start(lwu(OV(0x0015f5f4)) + static_cast<uint32_t>(idx) * 0x20, flags, 0, emitter + 0x40, volume);
    if (ch > -1) {
        const uint32_t e = 0x0013e5c0 + static_cast<uint32_t>(ch) * 0x70;
        sh(e + 0xe, static_cast<uint32_t>(idx));
        sw(e + 0x1c, emitter);
    }
    return ch;
}

namespace {
// the emitter's next start: `lo` + rand(hi - lo) seconds (params +4 / +8) into its timer +0xc
// orig: (helper) repeated in 0x002ed950 / 0x002edad8
void emitter_rearm(uint32_t prm) {
    if (lw(prm + 8) <= 0) return;
    const int32_t r = rand_mod(lw(prm + 8) - lw(prm + 4));
    const int32_t t = frame_scale_i(lw(prm + 4) + r);
    sw(prm + 0xc, static_cast<uint32_t>(static_cast<int32_t>(static_cast<float>(t) * 60.0f)));
}
// whether the emitter's sound still plays in its slot (params +0x10)
// orig: (helper) repeated in 0x002ed950 / 0x002edad8
bool emitter_playing(uint32_t em, uint32_t prm) {
    const uint32_t e = 0x0013e5c0 + lwu(prm + 0x10) * 0x70;
    return lwu(e + 0x1c) == em && lbu(e + 4) != 0;
}
}  // namespace

// A sphere sound emitter (params +8: sound +0, interval +4..+8, timer +0xc, slot +0x10): within
// its radius +0xc of the camera its sound starts when the timer allows; leaving it (1 unit
// past) stops it.
// orig: 0x002ed950
void emitter_sphere_update(uint32_t em) {
    const uint32_t prm = lwu(em + 8);
    if (lw(prm) < 0) return;
    const float d = vec3_distance(V(OV(0x00166dc0)), V(em + 0x40));
    const float r = lwc1(em + 0xc);
    if (d < r) {
        const uint32_t flags = lbu(lwu(OV(0x0015f5f4)) + lwu(prm) * 0x20 + 0x18) != 0 ? 4u : 0u;
        if (emitter_playing(em, prm)) return;
        int32_t& timer = *static_cast<int32_t*>(ee_ram_ptr(prm + 0xc));
        if (timer_tick_i32(timer) == 0) return;
        sw(prm + 0x10, static_cast<uint32_t>(level_sound_play(lw(prm), flags, em, 0x400)));
        emitter_rearm(prm);
        return;
    }
    if (!(r + 1.0f < d)) return;
    const int32_t ch = lw(prm + 0x10);
    if (ch != -1 && emitter_playing(em, prm)) sub_0028d918(ch);
    sw(prm + 0x10, 0xffffffffu);
}

// A box sound emitter (half size +0xc, turned by +0x10, its inverse +0x50, at +0x40): with the
// camera inside (1 unit of slack) its sound starts at a random point of the box.
// orig: 0x002edad8
void emitter_box_update(uint32_t em) {
    const uint32_t prm = lwu(em + 8);
    Vec4 d = Vec4{};
    vec3_sub(d, V(OV(0x00166dc0)), V(em + 0x40));
    d.w = 0.0f;
    const float h = lwc1(em + 0xc);
    const Vec4 ext = Vec4{h, h, h, 0.0f};
    Vec4 a = Vec4{}, b = Vec4{};
    mat3_transform(a, d, M(em + 0x50));
    mat3_transform(b, ext, M(em + 0x50));
    if (!(abs_f(a.x) <= abs_f(b.x) + 1.0f)) return;
    if (!(abs_f(a.y) <= abs_f(b.y) + 1.0f)) return;
    if (!(abs_f(a.z) <= abs_f(b.z) + 1.0f)) return;
    if (emitter_playing(em, prm)) return;
    int32_t& timer = *static_cast<int32_t*>(ee_ram_ptr(prm + 0xc));
    if (timer_tick_i32(timer) == 0) {
        sw(prm + 0x10, 0xffffffffu);
        return;
    }
    const int32_t ch = level_sound_play(lw(prm), 0, em, 0x400);
    sw(prm + 0x10, static_cast<uint32_t>(ch));
    if (ch != -1) {
        Vec4 p = Vec4{};
        p.x = rand_range_f(-1.0f, 1.0f);
        p.y = rand_range_f(-1.0f, 1.0f);
        p.z = rand_range_f(-1.0f, 1.0f);
        mat3_transform(p, p, M(em + 0x10));
        vec3_add(p, p, V(em + 0x40));
        V(0x0013e5c0 + static_cast<uint32_t>(lw(prm + 0x10)) * 0x70 + 0x20) = p;
    }
    emitter_rearm(prm);
}

// The level's sound emitters (0x13f2e4, 0x13f2e0 of them, 0x90 each): their update functions
// (+4: 0x2ed950 sphere, 0x2edad8 box).
// orig: 0x0028dfc8
void sound_emitters_update() {
    for (int32_t i = 0; i < lw(0x0013f2e0); ++i) {
        const uint32_t em = lwu(0x0013f2e4) + static_cast<uint32_t>(i) * 0x90;
        const uint32_t fn = lwu(em + 4);
        if (fn == 0) continue;
        if (fn == OVF(0x002ed950)) emitter_sphere_update(em);
        else if (fn == OVF(0x002edad8)) emitter_box_update(em);
        else if (on_eudora() && fn == L4_ADDR(0x002f1988)) eud_music_box_emitter_l4(em);  // Eudora's own emitter type (a box that starts the music)
        else if (on_aridia() && fn == L2_ADDR(0x002fc6f0)) sound_emitter_2fc6f0_l2(em);  // (Aridia's own emitter types)
        else if (on_aridia() && fn == L2_ADDR(0x002fc850)) sound_emitter_2fc850_l2(em);
        else if (on_batalia() && batalia_sound_emitter_call(fn, em)) {}  // (level 8's own emitter types 1 / 3 / 4 / 6)
        else if (on_umbris() && umbris_sound_emitter_call(fn, em)) {}  // (level 7's own emitter types 3 / 6)
        else if (on_orxon() && orxon_sound_emitter_call(fn, em)) {}  // (level 10's own emitter type 3)
        else if (on_pokitaru() && pokitaru_sound_emitter_call(fn, em)) {}  // (level 11's own emitter types 1 / 5)
        else if (on_hoven() && hoven_sound_emitter_call(fn, em)) {}  // (level 12's own emitter types 1 / 3 / 5 / 6)
        else if (on_gemlik() && gemlik_sound_emitter_call(fn, em)) {}  // (level 13's own emitter type 1)
        else if (on_oltanis() && oltanis_sound_emitter_call(fn, em)) {}  // (level 14's own emitter types 1 / 3 / 4 / 6)
        else if (on_quartu() && quartu_sound_emitter_call(fn, em)) {}  // (level 15's own emitter types 3 / 5)
        else if (on_kalebo() && kalebo_sound_emitter_call(fn, em)) {}  // (level 16's own emitter types 1 / 3 / 6)
        else if (on_veldin_orbit() && veldin_orbit_sound_emitter_call(fn, em)) {}  // (level 17's own emitter type 5)
        else if (on_veldin_finale() && veldin_finale_sound_emitter_call(fn, em)) {}  // (level 18's own emitter types 3 / 5 / 6)
        else if (on_gaspar() && gaspar_sound_emitter_call(fn, em)) {}  // (level 9's own emitter types 1 / 3)
        else if (on_rilgar() && rilgar_sound_emitter_call(fn, em)) {}  // (Rilgar's own emitter types 1 / 3 / 5 / 6)
        else if (on_kerwan() && kerwan_sound_emitter_call(fn, em)) {}  // (Kerwan's own emitter types)
        else if (!level_sound_emitter_call(fn, em)) TODO_PORT(fn, "sound emitter update")(em);  // (a level's own emitter)
    }
}

// The music / dialog streams' state 0x1516d0.. reset and 989snd's stream system started (4
// streams, 0xf000 bytes), its callback registered.
// orig: 0x00264b68
void stream_system_init() {
    const uint32_t s = 0x001516d0;
    sb(s + 0x22, 0xff);
    sb(s + 0x30, 0x20);
    sw(s, 0);
    sb(s + 0x31, 0);
    sb(s + 0x32, 0);
    sb(s + 0x33, 0);
    sw(s + 0x34, 0);
    sh(s + 0x3e, 0);
    sw(s + 0x50, 0);
    sh(s + 0x5a, 0);
    sw(s + 0x6c, 0);
    sh(s + 0x76, 0);
    sw(s + 0x1c, 0xffffffffu);
    snd_stream_init(4, 0xf000, 0, 1);
    while (snd_flush() != 0) {
    }
    sub_00264bf8();
}

// The game's sound start (its main's): the listener's positions and the slots cleared, 989snd
// up (mono 0x15ede8, its settings), the streams, the group volumes from the options' sound
// 0x15edf0 (0, 2: 8/10; 3, 4: 7/10; 5: all) and music 0x15edec (1).
// orig: 0x0028ca88
void sound_system_init() {
    for (uint32_t k = 0; k < 4; ++k) V(0x0013e550 + k * 0x10) = Vec4{};
    sw(0x0013e590, 0);
    sw(0x0013e594, 0);
    for (uint32_t e = 0x0013e5c0; e < 0x0013f2e0; e += 0x70) {
        sw(e, 0);
        sb(e + 4, 0);
    }
    snd_init();
    snd_cmd_0b(lw(0x0015ede8) == 0 ? 1u : 0u);
    snd_cmd_0d(0, 1);
    snd_cmd_51(2, 4);
    snd_cmd_4e(1, 0x18, 0x2f);
    snd_cmd_4e(2, 0x18, 0x2f);
    snd_cmd_4e(4, 0x18, 0x2f);
    const int32_t sfx = lw(0x0015edf0);
    sw(0x0013e59c, lwu(0x0015edec));
    sw(0x0013e598, static_cast<uint32_t>(sfx * 8 / 10));
    sw(0x0013e5a0, static_cast<uint32_t>(sfx * 8 / 10));
    sw(0x0013e5a4, static_cast<uint32_t>(sfx * 7 / 10));
    sw(0x0013e5a8, static_cast<uint32_t>(sfx * 7 / 10));
    sw(0x0013e5ac, static_cast<uint32_t>(sfx));
    stream_system_init();
    snd_set_master_volume(0, lw(0x0013e598));
    snd_set_master_volume(1, lw(0x0013e59c));
    snd_set_master_volume(2, lw(0x0013e5a0));
    snd_set_master_volume(3, lw(0x0013e5a4));
    snd_set_master_volume(4, lw(0x0013e5a8));
    snd_set_master_volume(5, lw(0x0013e5ac));
}

// Every sound stopped (the level restarts): the batch sent, 989snd's sounds stopped and waited
// for, the listener's positions and every slot cleared.
// orig: 0x0028e048
void sound_stop_all() {
    snd_release();
    snd_flush();
    snd_cmd_18();
    while (snd_flush() != 0) {
    }
    for (uint32_t k = 0; k < 4; ++k) V(0x0013e550 + k * 0x10) = Vec4{};
    sw(0x0013e590, 0);
    for (uint32_t e = 0x0013e5c0; e < 0x0013f2e0; e += 0x70) {
        sw(e, 0);
        sb(e + 4, 0);
    }
}

// orig: (none) — the EE callbacks 989snd's replies go to, by address
void sound_callbacks_register() {
    snd_register_callback(0x0022dd78, snd_reply_store);
    snd_register_callback(0x0028e0f0, snd_reply_store);  // the same code in the overlay
    snd_register_callback(0x0028e108, snd_play_reply);
    snd_register_callback(0x0028e150, snd_set_reply);
    music_streams_callbacks_register();
}

}  // namespace rac1
