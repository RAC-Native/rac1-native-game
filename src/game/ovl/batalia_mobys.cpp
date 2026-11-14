// Batalia (level 8) own moby classes: the registration table. The ports are the transliterations in
// batalia_classes_tr_N.cpp (declarations: batalia_mobys.h). Keyed by level 8's own address and by level, so the indirect call (`jalr` through
// moby +0x74) reaches them only while the played level's overlay is level 8's.
#include <game/ovl/batalia_hooks.h>
#include <game/ovl/batalia_mobys.h>
#include <game/ovl/batalia_part2.h>

#include <game/ovl/carry_hooks.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L8:0x002d3f60 (class hook +0: the indirect-call form, the moby only)
void bat_hook_2d3f60_m(uint32_t m) { enemy10e_hook0(m, 0, 0, 0.0f); }  // (Veldin's 0x2cadc0, the same code)

// orig: (none) -- adapter of L8:0x002d4000 (class hook +8: the indirect-call form, the moby only)
void bat_hook_2d4000_m(uint32_t m) { bat_hook_2d4000_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L8:0x002d4050 (class hook +0xc: the indirect-call form, the moby only)
void bat_hook_2d4050_m(uint32_t m) { bat_hook_2d4050_u(m, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) -- adapter of L8:0x002d8180 (class hook +0xc of the class 270 table: the indirect-call form, the moby only)
void bat_hook_2d8180_m(uint32_t m) { bat_carry_hookc_l8(m, 0, 0, 0.0f); }

}  // namespace

// orig: (none) -- level 8's registrations
void batalia_moby_fns_register() {
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002b82a0), bat_class152_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002ba8d0), bat_class156_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d23f0), bat_class248_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d2af0), bat_class252_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d42d8), bat_class253_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d8270), bat_class280_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002da0c8), empty_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002da0f0), bat_class327_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002da3c0), bat_class331_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dabf0), bat_class333_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dba40), bat_class344_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dbdb0), bat_class424_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dc9a8), bat_class425_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dd3c0), bat_class435_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002de848), bat_class438_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e0328), bat_class440_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e16c0), bat_class441_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e24e8), bat_class444_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e2df0), bat_class452_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e6530), bat_class455_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e8210), bat_class458_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e8cd0), bat_class462_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e9b70), bat_class463_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002ea398), bat_class467_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002ea4c0), bat_class468_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002ee6f0), bat_class545_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002efb50), empty_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002efb60), bat_class567_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f1548), bat_class600_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f18b8), bat_class604_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f1c08), bat_class605_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f2030), bat_class615_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f2948), bat_trespasser_puzzle_draw_l8);  // (the Trespasser pad's draw-list callback)
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f44b8), empty_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f44c0), bat_class621_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f5830), bat_class648_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f70a0), bat_class671_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f74c0), bat_class679_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f90c8), bat_class774_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f99b0), bat_class805_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002fbb58), bat_class832_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00301158), bat_class1023_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00302a78), bat_class1039_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00302ce8), bat_class1130_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00303370), bat_class1134_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003042a8), bat_class1135_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00305270), bat_class1144_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003065d8), bat_class1283_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00307298), bat_class1292_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00307540), bat_class1349_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00307cf0), bat_class1400_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003084d8), bat_class1553_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003085f0), bat_class1629_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00308920), bat_class1633_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00308c90), bat_class1641_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003097b0), bat_class1649_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x0030ac28), bat_class1818_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x0030bc00), bat_class1844_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x0030bcf8), empty_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d3f60), bat_hook_2d3f60_m);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d4000), bat_hook_2d4000_m);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d4050), bat_hook_2d4050_m);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d8180), bat_hook_2d8180_m);
    batalia_part2_register();  // (the classes' draw-list callbacks)
}

}  // namespace rac1
