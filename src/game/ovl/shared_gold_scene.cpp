// The gold weapons' scene start of the gold weapon cases of Novalis (level 1) and Gemlik Base (13): the sound and the
// dialogue stopped, the scene's record (Veldin 0x1ca4c0, 0x220 bytes) set up for the weapon (scene 3, the case as its
// moby), Ratchet put in the scene's state 0x64 and placed before the case (the case's offset 0x161100 turned by it, on the
// floor), the hero's held weapon closed, the weapon's class loaded and its scene's data streamed in (the vendor's
// 0x2454c8 / 0x2455c0), waiting for the stream, and the weapon handed to Ratchet's slot. The two levels' copies are the same
// code, relinked (all their words are the engine's: OV(), or the base ELF's), apart from Ratchet's state: each level's
// hero_set_state (Novalis's own build). Header shared_gold_scene.h.
#include <game/ovl/shared_gold_scene.h>

#include <game/lib/snd989.h>
#include <game/math/vecmath.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/vendor_tr.h>
#include <runtime/ee/ee_thread.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t STREAMS = 0x001516d0;  // (the music / dialogue streams: +0x1c the next scene's data, +0x5a the state)
}  // namespace

// orig: L1:0x002aea70 L13:0x002a5390
void gold_weapon_scene(uint32_t m, uint32_t weapon) {
    const uint32_t scene = OV(0x001ca4c0);
    const uint32_t draw = OV(0x0016c860);
    snd_cmd_16(0x1d);
    sub_00265828(0);
    snd_flush();
    sw(OV(0x0015f5d8), 1);
    mem_fill32(scene, 0, 0x220);
    sw(scene, 3);
    sw(OV(0x0015f5c4), 5);
    sw(scene + 4, 0);
    sw(scene + 0x28, m);
    camera_reset();
    sub_002039a0();
    if (on_novalis()) hero_set_state_l1(0x64, 1);
    else hero_set_state(0x64, 1);
    sb(HERO + 0x20a5, 1);
    sb(HERO + 0x20ac, 1);
    sub_00232fe8();
    const uint32_t place = scene + 0xc0, turn = scene + 0xb0;
    mat3_transform(V(place), V(OV(0x00161100)), M(m + 0xc0));
    vec3_add(V(place), V(place), V(m + 0x10));
    swc1(scene + 0xc8, floor_probe_v(0.5f, V(place), 0));
    vec_clear(V(turn));
    swc1(scene + 0xb8, angle_add(lwc1(m + 0x48), -u2f(0x3fc90fdbu)));  // (facing the case: a quarter turn from it)
    euler_to_mat3_vu(M(scene + 0x80), V(turn));
    mem_fill32(draw, 0, 0x1c0);
    mem_fill32(OV(0x0017c340), 0, 0x40);
    sub_002a07c8();
    const uint32_t heap = lwu(OV(0x001611cc)) + 0xfffc0000u;  // (0x40000 of the heap's top for the scene)
    const uint32_t frame_bufs = OV(0x00173e00);
    sw(draw + 0x5c, lwu(frame_bufs + 8) + heap);
    const uint32_t data = lwu(OV(0x001ca020) + (weapon << 2));
    sw(draw + 0x58, lwu(frame_bufs + 4) + heap);
    swc1(OV(0x0015f3fc), 1.0f);
    sw(OV(0x001611cc), heap);
    sw(OV(0x0015f400), 0);
    hero_slot_close(0, 0);
    sw(HERO + 0x20b8, weapon);
    item_class_load(lwu(OV(0x00179ac0) + weapon * 0x4c + 0x10), -1);
    ovl_2454c8_tr(data, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);  // (reads its a0 only)
    sw(STREAMS + 0x1c, 0x2734);
    ovl_2455c0_tr(0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    while (lh(STREAMS + 0x5a) != 3) {  // (the scene's data streamed in)
        sound_frame();
        ee_thread_vsync(EeVsyncFrame::Held, true);  // (the vsync wait of the stream's load loop: nothing new is drawn, a vsync passes, ADR 0012)
    }
    sub_00207330(lwu(draw + 0x17c), lwu(HERO + 0x2080));
    dialog_stream_stop();
}

}  // namespace rac1
