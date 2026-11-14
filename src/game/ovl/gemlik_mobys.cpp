// Gemlik Base (level 13) own moby classes: the registration table. The ports are the transliterations in
// gemlik_classes_tr_N.cpp (declarations: gemlik_mobys.h). Keyed by level 13's own address and by level, so the indirect call (`jalr`)
// through moby +0x74 (or a draw list, a HUD element) reaches them only while the played level's overlay is level 13's.
#include <game/ovl/gemlik_hooks.h>
#include <game/ovl/gemlik_mobys.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L13:0x002b7980 (class 63's hook +0: the indirect-call form, the moby only)
void gml_hook_2b7980_m(uint32_t m) { gml_hook_2b7980_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L13:0x002b7a20 (class 63's hook +8: the indirect-call form, the moby only)
void gml_hook_2b7a20_m(uint32_t m) { gml_hook_2b7a20_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L13:0x002b7ad0 (class 63's hook +0xc: the indirect-call form, the moby only)
void gml_hook_2b7ad0_m(uint32_t m) { gml_hook_2b7ad0_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }
}  // namespace

// orig: (none) -- level 13's registrations
void gemlik_moby_fns_register() {
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b0478), gml_class726_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b0e80), gml_class6_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b39e0), gml_class21_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b41b8), gml_class29_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b4c80), gml_class36_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b50d8), gml_class63_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002bb068), gml_class69_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002c1528), gml_class82_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002c2048), gml_class83_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002c25b8), gml_class101_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002c32d0), gml_class104_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002c4428), gml_class111_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002c7f38), gml_class127_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002c8390), gml_class129_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002cefc0), gml_class170_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002e1638), gml_class212_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002e1cc8), gml_class224_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002e4448), gml_class231_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002e4bd0), gml_class258_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002e6c08), gml_class295_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002e7238), gml_class304_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002e84d8), gml_class352_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002eb098), gml_class388_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002ece00), gml_class389_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002ed4a8), gml_class404_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002edbe8), gml_class447_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002f3750), gml_class533_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002f3778), gml_class558_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002f51f0), gml_class605_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002f5618), gml_class615_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002f8880), gml_class667_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002f8a78), gml_class674_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002f8e70), gml_class737_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002fa708), gml_class758_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002fac70), gml_class805_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002fce18), gml_class832_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00302418), empty_update);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00302780), gml_class1009_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00302e48), gml_class1017_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00303128), gml_class1034_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x003036f0), gml_class1039_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00303878), gml_class1085_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00303950), gml_class1134_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00304888), gml_class1135_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00305a58), gml_class1233_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00306300), gml_class1238_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00307b10), gml_class1261_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00307e98), gml_class1262_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00309e88), gml_class1263_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030a6d8), gml_class1270_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030af50), gml_class1271_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030b628), gml_class1353_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030bb70), gml_class1403_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030bdf0), gml_class1558_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030be60), gml_class1577_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030c060), gml_class1632_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030c3b8), gml_class1634_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030cdb8), gml_class1805_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030d550), gml_class1885_update_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030dd80), gml_class1886_update_l13);
    // the classes' draw-list callbacks (queued with draw_list_add / L13:0x204dc8, 0x204f80; run by the frame's draw lists) and the HUD
    // element 0x11's draw function (L13:0x245cf8, requested by class 388's update with camera_request)
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b97f8), gml_cb_2b97f8_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002ea8c8), gml_cb_2ea8c8_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002f5f30), gml_cb_2f5f30_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002fcc08), gml_cb_2fcc08_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x003051a0), gml_cb_3051a0_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x003098d0), gml_cb_3098d0_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x0030a460), gml_cb_30a460_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x00245cf8), gml_hud_245cf8_l13);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b7980), gml_hook_2b7980_m);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b7a20), gml_hook_2b7a20_m);
    ovl_register_level_moby_fn(GEMLIK, L13_ADDR(0x002b7ad0), gml_hook_2b7ad0_m);
}

}  // namespace rac1
