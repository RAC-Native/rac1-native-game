// Batalia (level 8) own moby classes: the registration table. The ports are the transliterations in
// batalia_classes_tr_N.cpp (declarations: batalia_mobys.h) and, for the classes several levels carry, the hand-written
// shared ports (shared_*.cpp, novalis_*.cpp, aridia_props.cpp). Keyed by level 8's own address and by level, so the indirect call (`jalr` through
// moby +0x74) reaches them only while the played level's overlay is level 8's.
#include <game/ovl/batalia_hooks.h>
#include <game/ovl/batalia_mobys.h>
#include <game/ovl/batalia_part2.h>

#include <game/ovl/carry_hooks.h>
#include <game/ovl/carry_hooks_levels.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/shared_class1023.h>
#include <game/ovl/shared_class156.h>
#include <game/ovl/shared_class1400.h>
#include <game/ovl/shared_class1629.h>
#include <game/ovl/shared_class1844.h>
#include <game/ovl/shared_class252.h>
#include <game/ovl/shared_class280.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/shared_class567.h>
#include <game/ovl/shared_class604.h>
#include <game/ovl/shared_class615.h>
#include <game/ovl/shared_class679.h>
#include <game/ovl/shared_class832.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_homing_missile.h>
#include <game/ovl/shared_puffer.h>
#include <game/ovl/shared_sandmouse.h>
#include <game/ovl/shared_spawner.h>
#include <game/ovl/shared_tracer.h>
#include <game/ovl/shared_zones.h>

namespace rac1 {

namespace {
// orig: (none) -- adapter of L8:0x002d3f60 (class hook +0: the indirect-call form, the moby only)
void bat_hook_2d3f60_m(uint32_t m) { enemy10e_hook0(m, 0, 0, 0.0f); }  // (Veldin's 0x2cadc0, the same code)

// orig: (none) -- adapter of L8:0x002d4000 (class hook +8: the indirect-call form, the moby only)
void bat_hook_2d4000_m(uint32_t m) { carry_state_hook(m, 0, 0, 0.0f, 8, 5, 1); }  // (shared_helpers.cpp: hook +8, carried 5, released 1)

// orig: (none) -- adapter of L8:0x002d4050 (class hook +0xc: the indirect-call form, the moby only)
void bat_hook_2d4050_m(uint32_t m) { class252_hook_c(m, 0, 0, 0.0f); }  // (shared_helpers.cpp)

// orig: (none) -- adapter of L8:0x002d8180 (class hook +0xc of the class 270 table: the indirect-call form, the moby only)
void bat_hook_2d8180_m(uint32_t m) { bat_carry_hookc_l8(m, 0, 0, 0.0f); }

}  // namespace

// orig: (none) -- level 8's registrations
void batalia_moby_fns_register() {
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002b82a0), bat_class152_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002ba8d0), class156_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d23f0), bat_class248_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d2af0), class252_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d42d8), bat_class253_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d8270), class280_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002da0c8), empty_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002da0f0), bat_class327_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002da3c0), trackdrive_left_update_l2);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dabf0), bat_class333_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dba40), class344_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dbdb0), bat_class424_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dc9a8), bat_class425_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002dd3c0), bat_class435_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002de848), bat_class438_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e0328), bat_class440_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e16c0), bat_class441_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e24e8), bat_class444_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e2df0), bat_class452_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e6530), class455_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e8210), homing_missile_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e8cd0), bat_class462_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002e9b70), bat_class463_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002ea398), bat_class467_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002ea4c0), bat_class468_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002ee6f0), class545_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002efb50), empty_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002efb60), class567_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f1548), bat_class600_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f18b8), class604_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f1c08), zone_marker_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f2030), class615_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f2948), class615_puzzle_draw);  // (the Trespasser pad's draw-list callback)
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f44b8), empty_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f44c0), bat_class621_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f5830), puffer_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f70a0), bat_class671_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f74c0), class679_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f90c8), bat_class774_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002f99b0), spawn_point_update_l1);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002fbb58), class832_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00301158), class1023_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00302a78), deadly_zones_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00302ce8), bat_class1130_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00303370), gold_bolt_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003042a8), teleporter_update_l1);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00305270), bat_class1144_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003065d8), bat_class1283_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00307298), tracer_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00307540), bat_class1349_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00307cf0), class1400_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003084d8), bat_class1553_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003085f0), class1629_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00308920), class1633_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x00308c90), bat_class1641_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x003097b0), bat_class1649_update_l8);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x0030ac28), sandmouse_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x0030bc00), class1844_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x0030bcf8), empty_update);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d3f60), bat_hook_2d3f60_m);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d4000), bat_hook_2d4000_m);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d4050), bat_hook_2d4050_m);
    ovl_register_level_moby_fn(BATALIA, L8_ADDR(0x002d8180), bat_hook_2d8180_m);
    batalia_part2_register();  // (the classes' draw-list callbacks)
}

}  // namespace rac1
