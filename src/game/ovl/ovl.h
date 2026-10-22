#ifndef RAC1_GAME_OVL_OVL_H
#define RAC1_GAME_OVL_OVL_H

// Ported overlay functions (Veldin, NTSC-U), grouped in files by address
// range (ovl_00XX.cpp ~ 0x00XX0000..0x00XXffff), which roughly follows the
// original translation units. Names: descriptive where the role is clear,
// sub_<address> otherwise. Each one is verified against the original with the
// EE interpreter (ADR 0006).
//
// EE pointers are passed as uint32_t addresses; game data lives in EE RAM.

#include <cstdint>

#include <game/math/vecmath.h>  // Vec4, Mat4

namespace rac1 {

// ---- indirect calls (ovl_dispatch.cpp) --------------------------------------
using MobyFn = void (*)(uint32_t moby);
void ovl_register_moby_fn(uint32_t addr, MobyFn fn);     // port table for `jalr` targets
void ovl_call_moby_fn(uint32_t fn, uint32_t moby);       // `jalr fn` with a0 = moby
bool ovl_moby_fn_ported(uint32_t addr);
MobyFn ovl_moby_fn_resolve(uint32_t addr);               // (host) the port `jalr addr` calls (nullptr: none)

// ---- 0x001exxxx -------------------------------------------------------------
uint32_t moby_table_find_by_uid(int32_t uid);            // orig: 0x001eb1f8
void debug_printf_stub();                                // orig: 0x001eb078
void sub_001eb128();                                     // orig: 0x001eb128
int32_t sub_001ed630(uint32_t out, uint32_t a, uint32_t b);  // orig: 0x001ed630
void sub_001fa050(int64_t r, int64_t g, int64_t b, int64_t a);  // orig: 0x001fa050

// ---- 0x001fxxxx -------------------------------------------------------------
int32_t sub_001f35d8();                                  // orig: 0x001f35d8
int32_t sub_001f3608();                                  // orig: 0x001f3608
void sub_001f6c20();                                     // orig: 0x001f6c20
void sub_001f6c30(uint32_t pos, float r);                // orig: 0x001f6c30
void sub_001f93e0(uint32_t a, uint32_t b);               // orig: 0x001f93e0

// ---- 0x0020xxxx -------------------------------------------------------------
void sub_00203a28();                                     // orig: 0x00203a28
uint32_t sub_002050b8(uint32_t v);                       // orig: 0x002050b8
uint32_t sub_00205110(int32_t kind);                     // orig: 0x00205110
void fx_params_set_0(float a, float b);                  // orig: 0x0020a1e0
void fx_params_set_1(float a, float b);                  // orig: 0x0020a1f8
void fx_params_set_2(float a, float b);                  // orig: 0x0020a210
void fx_params_set_3(float a, float b);                  // orig: 0x0020a228
void sub_0020a500();                                     // orig: 0x0020a500
uint32_t sub_0020d460(int32_t slot);                     // orig: 0x0020d460
int32_t sub_0020d498(int32_t slot);                      // orig: 0x0020d498
int32_t sub_0020d568();                                  // orig: 0x0020d568
uint32_t sub_0020d5b8(uint32_t moby, uint32_t v);        // orig: 0x0020d5b8
float sub_0020d658(int32_t back);                        // orig: 0x0020d658
int32_t sub_0020da10(int32_t v);                         // orig: 0x0020da10
void sub_00200fd8();                                     // orig: 0x00200fd8
void sub_00201040();                                     // orig: 0x00201040
void sub_002039a0();                                     // orig: 0x002039a0
int32_t sub_00203a68(int32_t v);                         // orig: 0x00203a68
int32_t sub_00204af8(int32_t key, int32_t field, uint32_t out);  // orig: 0x00204af8
void sub_002072c8();                                     // orig: 0x002072c8
void sub_00207d00();                                     // orig: 0x00207d00
int32_t sub_0020d4d0(int32_t slot);                      // orig: 0x0020d4d0
int32_t sub_0020d518(int32_t slot);                      // orig: 0x0020d518
int32_t sub_0020d540(int32_t slot);                      // orig: 0x0020d540
int32_t sub_0020d5d0(int32_t kind, float* out);          // orig: 0x0020d5d0
void hero_layers_update();                                // orig: 0x0020dd20
void hero_layers_start(int32_t off, int32_t on);          // orig: 0x0020e360
void hero_extra_layer_update();                           // orig: 0x0020da68
void sub_0020fd80(int32_t kind);                         // orig: 0x0020fd80
void sub_0020fde0(int32_t kind);                         // orig: 0x0020fde0
int32_t sub_0020fe30();                                  // orig: 0x0020fe30
uint32_t sub_00203ad8(int32_t v);                        // orig: 0x00203ad8
void sub_00208038_v(const Vec4& vec, uint32_t v, float a, float b);  // orig: 0x00208038
void sub_00208038(uint32_t vec, uint32_t v, float a, float b);  // orig: 0x00208038
void sub_0020d2f8_v(Vec4& dst);                         // orig: 0x0020d2f8
void sub_0020d2f8(uint32_t dst);                         // orig: 0x0020d2f8
float mat3_abs_diff(uint32_t a, uint32_t b);             // orig: 0x0020f0b8
uint32_t sub_00205000(int32_t flag);                     // orig: 0x00205000
void sub_00205538();                                     // orig: 0x00205538
void sub_00207c48();                                     // orig: 0x00207c48
float sub_00209018(uint32_t table, int32_t n);           // orig: 0x00209018
void sub_00205600();                                     // orig: 0x00205600
void sub_00206c08();                                     // orig: 0x00206c08
void sub_002079e8();                                     // orig: 0x002079e8
void sub_00204b60(int32_t key);                          // orig: 0x00204b60
void sub_00207330(uint32_t target, uint32_t m);          // orig: 0x00207330
void sub_00208f20(float x, float a, float b, float c);   // orig: 0x00208f20
int32_t sub_00203908(int32_t v, int32_t opt);            // orig: 0x00203908
void sub_00206cf8();                                     // orig: 0x00206cf8
void sub_002090d0_v(const Vec4& pivot, float x, float y, float z);  // orig: 0x002090d0
void sub_002090d0(uint32_t pivot, float x, float y, float z);  // orig: 0x002090d0
void sub_002091d8(float x, float y, float z);            // orig: 0x002091d8
void sub_00209540(int32_t n, int32_t limit);             // orig: 0x00209540
void sub_00207a58();                                     // orig: 0x00207a58
void sub_00209848(int32_t n, int32_t limit);             // orig: 0x00209848
void sub_002076a8();                                     // orig: 0x002076a8
void sub_00209638(int32_t n, int32_t limit);             // orig: 0x00209638
float target_score(uint32_t t, int32_t* rejected, float yaw, float max_dist, float max_yaw, float max_pitch);  // orig: 0x0020d6b0
uint32_t find_best_target(float max_dist, float yaw, float max_yaw, float max_pitch);  // orig: 0x0020d8f8
void sub_0020a9e8();                                     // orig: 0x0020a9e8
void cheat_input();                                      // orig: 0x00206e00
void sub_002078a8();                                     // orig: 0x002078a8

// ---- 0x0021xxxx -------------------------------------------------------------
int32_t moby_is_class_0x47(uint32_t moby);               // orig: 0x00211360
int32_t sub_00211870(float v);                           // orig: 0x00211870
void sub_00216ec0(int16_t a, int16_t b, int16_t c);      // orig: 0x00216ec0
void sub_00210c80(uint32_t v, int32_t flag, float s, float z);  // orig: 0x00210c80
void sub_00210c80_v(Vec4& v, int32_t flag, float s, float z);  // orig: 0x00210c80
float sub_00211830();                                    // orig: 0x00211830
float sub_00213350_v(const Vec4& v);                     // orig: 0x00213350
float sub_00213350(uint32_t v);                          // orig: 0x00213350
void sub_00213de8(float s);                              // orig: 0x00213de8
float sub_00212088(float up, float down);                // orig: 0x00212088
void sub_002132b8(float s, float dead);                  // orig: 0x002132b8
void sub_00216078_v(const Vec4& v, float* out);           // orig: 0x00216078
void sub_00216078(uint32_t v, float* out);               // orig: 0x00216078
void sub_002120d8(float yaw);                            // orig: 0x002120d8
void sub_00216710(float dead);                           // orig: 0x00216710
void sub_002126b8_v(Vec4& a, Vec4& b, int32_t flag, float ha, float hb);  // orig: 0x002126b8
void sub_002126b8(uint32_t a, uint32_t b, int32_t flag, float ha, float hb);  // orig: 0x002126b8
void sub_00211cf8(float x, float y, float z);            // orig: 0x00211cf8
void sub_002121c0();                                     // orig: 0x002121c0
void sub_00215ef8();                                     // orig: 0x00215ef8
void sub_00211d78(int32_t dir, float k, float d, float vmax);  // orig: 0x00211d78
void sub_00217450();                                     // orig: 0x00217450
void sub_00211e30(int32_t dir, float k, float d, float vmax);  // orig: 0x00211e30
void sub_00211e98();                                     // orig: 0x00211e98
void sub_00211f38();                                     // orig: 0x00211f38
void sub_00213e68(float dt);                             // orig: 0x00213e68
int32_t hero_sound_start(int32_t idx, uint32_t flags);   // orig: 0x00216de8
void hero_hold_moby_start();                              // orig: 0x00217658
void hero_sound_slot(int32_t slot, uint32_t owner, int32_t idx);  // orig: 0x00216e48
void wrench_hit_sparks(uint32_t moby, int32_t n);        // orig: 0x00217368
void align_to_up_v(const Vec4& up, Mat4& m, uint32_t out_ang, float k, float d, float vmax);  // orig: 0x00216130
void align_to_up(uint32_t up, uint32_t m, uint32_t out_ang, float k, float d, float vmax);  // orig: 0x00216130
void sub_00212ff0();                                     // orig: 0x00212ff0
void sub_002163f0();                                     // orig: 0x002163f0
void hero_delayed_sounds();                              // orig: 0x00216f10
void hero_tilt_update();                                 // orig: 0x002165b8
void hero_gait_update();                                 // orig: 0x00211380

// ---- 0x0022xxxx -------------------------------------------------------------
void sub_00221b68();                                     // orig: 0x00221b68
void sub_00227d40();                                     // orig: 0x00227d40

// ---- 0x0023xxxx -------------------------------------------------------------
void sub_002321a8();                                     // orig: 0x002321a8
int32_t sub_002321c0(int32_t key, int32_t* out);         // orig: 0x002321c0
void sub_002325e0(int32_t a, int32_t b);                 // orig: 0x002325e0
void sub_00232628();                                     // orig: 0x00232628
void hero_mobys_set_flag1();                             // orig: 0x00233210
void hero_mobys_clear_flag1();                           // orig: 0x00233248
void sub_00233288(int32_t a, int32_t b, int32_t c, int32_t d, float e, float f);  // orig: 0x00233288
uint32_t sub_00233e98(int32_t idx);                      // orig: 0x00233e98
int32_t sub_00233f38();                                  // orig: 0x00233f38
int32_t sub_00234e48(uint32_t key);                      // orig: 0x00234e48
void sub_002322c8();                                     // orig: 0x002322c8
void sub_00232640(int32_t frames);                       // orig: 0x00232640
void sub_00233660_v(Vec4& dst, float x, float y, float z);  // orig: 0x00233660
void sub_00233660(uint32_t dst, float x, float y, float z);  // orig: 0x00233660
void sub_00233b20_v(Vec4& dst, const Vec4& src, float z);  // orig: 0x00233b20
void sub_00233b20(uint32_t dst, uint32_t src, float z);  // orig: 0x00233b20
void sub_00233ba0_v(Vec4& dst, const Vec4& src, float len);  // orig: 0x00233ba0
void sub_00233ba0(uint32_t dst, uint32_t src, float len);  // orig: 0x00233ba0
float sub_00233d30(uint32_t v);                          // orig: 0x00233d30
int32_t sub_00233db8(int32_t idx, int32_t amount);       // orig: 0x00233db8
void sub_00235988(uint32_t obj, uint32_t key);
int32_t camera_request(uint32_t id, uint32_t key, uint32_t start, uint32_t cb2, uint32_t cb3, uint32_t arg, uint32_t arg2);  // orig: 0x00235700
void camera_slot_start(uint32_t e);                       // orig: 0x00235810
void camera_slots_reset();                                // orig: 0x00235878
void camera_reset();                                      // orig: 0x0023aef8
int32_t camera_request_7535();                               // orig: 0x00234e00
int32_t hero_set_state(uint32_t state, int32_t mode);     // orig: 0x002223f8
void hero_state_update();                                 // orig: 0x00217970
int32_t hero_try_item_stance();                          // orig: 0x002276c0
int32_t hero_try_jump(int32_t back, int32_t no_plain);   // orig: 0x00228b78
int32_t hero_water_air();                                // orig: 0x00227778
int32_t hero_water_entry();                              // orig: 0x002279b0
void hero_land();                                        // orig: 0x002293a8
int32_t hero_jump_input();                               // orig: 0x00229660
int32_t hero_hurt(int32_t knock);                        // orig: 0x00210ce8
int32_t hero_item_use();                                 // orig: 0x00227fa0
void hero_input_update();                                // orig: 0x00229b70
void hero_lost();                                        // orig: 0x00211250
void hero_stat_use(uint32_t rec);                        // (inlined in 0x2223f8 / 0x217970)
void hero_mode3_exit();                                   // orig: 0x0020b930
void hero_mode_exit();                                    // orig: 0x00210b30
void hero_mode_enter(uint32_t mode, uint32_t state, uint32_t moby);  // orig: 0x00210a08
void hero_timers_update();                                // orig: 0x002215c8
void hero_tick();                                         // orig: 0x00211670
void hero_surface_update();                               // orig: 0x0020b960
void hero_equipment_spawn();                              // orig: 0x0020ea80
void hero_items_update();                                 // orig: 0x00210928
void hero_frame_update();                                 // orig: 0x002070d0
void hero_init();                                         // orig: 0x00204c60
void hero_enter_idle();                                   // orig: 0x00227638
int32_t hero_item_idle();                                 // orig: 0x00229a98
void hero_counter_use(int32_t n);                         // orig: 0x002050c0
void hero_camera_7_update();                              // orig: 0x00205ea8           // orig: 0x00235988
uint32_t sub_002359f8(uint32_t key, uint32_t v);         // orig: 0x002359f8
int32_t sub_00235e18(uint32_t key, int32_t v);           // orig: 0x00235e18
void sub_00232320();                                     // orig: 0x00232320
int32_t sub_00233440();                                  // orig: 0x00233440
void sub_002335c8_v(Vec4& dst, float r, float a, float z);  // orig: 0x002335c8
void sub_002335c8(uint32_t dst, float r, float a, float z);  // orig: 0x002335c8
float hero_fall_time(float t, float g, float h);             // orig: 0x00232b90
float sub_00233a78_v(const Vec4& v);                     // orig: 0x00233a78
float sub_00233a78(uint32_t v);                          // orig: 0x00233a78
float sub_002339d0_v(const Vec4& v);                     // orig: 0x002339d0
float sub_002339d0(uint32_t v);                          // orig: 0x002339d0
int32_t sub_00231db0(uint32_t key, int32_t* out);        // orig: 0x00231db0
void sub_00233810_v(Vec4& dst, const Vec4& src, float h);  // orig: 0x00233810
void sub_00233810(uint32_t dst, uint32_t src, float h);  // orig: 0x00233810
void sub_00232fe8();                                     // orig: 0x00232fe8
void sub_002334d0_v(Vec4& dst, const Vec4& src, float h);  // orig: 0x002334d0
void sub_002334d0(uint32_t dst, uint32_t src, float h);  // orig: 0x002334d0
void sub_002336e8_v(Vec4& dst, const Vec4& src, float h);  // orig: 0x002336e8
void sub_002336e8(uint32_t dst, uint32_t src, float h);  // orig: 0x002336e8
void sub_00233708_v(Vec4& dst, const Vec4& src, float d);  // orig: 0x00233708
void sub_00233708(uint32_t dst, uint32_t src, float d);  // orig: 0x00233708
void sub_002338d0_v(Vec4& dst, const Vec4& src);         // orig: 0x002338d0
void sub_002338d0(uint32_t dst, uint32_t src);           // orig: 0x002338d0
float sub_00233c30(uint32_t pos, uint32_t vel, int32_t* frames_out, float g);  // orig: 0x00233c30
int32_t sub_00231ff8(uint32_t v, int32_t* a, int32_t* b);  // orig: 0x00231ff8
void hero_hands_anim(int32_t on, int32_t frame, float blend);  // orig: 0x00231e78
void hero_hands_anim_start();                             // orig: 0x00231d08
void hero_pair_anim(int32_t key, int32_t frame, float blend);  // orig: 0x00232220
void hero_anim_set(uint32_t set, uint32_t frame, float blend);  // orig: 0x002323b8
void hero_run_gait_update();                              // orig: 0x00228c18
void sub_00232128();                                     // orig: 0x00232128
void sub_002330d0();                                     // orig: 0x002330d0
float sub_002332d0();                                    // orig: 0x002332d0
void sub_00234808();                                     // orig: 0x00234808
int32_t sub_0023e5e0_v(const Vec4& pos, uint32_t rgb, float size, float intensity);  // orig: 0x0023e5e0
int32_t sub_0023e5e0(uint32_t pos, uint32_t rgb, float size, float intensity);  // orig: 0x0023e5e0

// ---- 0x0024xxxx / 0x0025xxxx -------------------------------------------------
int32_t sub_0024f698(uint32_t v);                        // orig: 0x0024f698
void moby_set_color_packed(uint32_t moby, int64_t a, int64_t b, int64_t c, int64_t d);  // orig: 0x002502d0 (5th arg in $t0)
void moby_set_color_hi(uint32_t moby, int64_t a, int64_t b, int64_t c);  // orig: 0x002502f0
void moby_get_color_hi_v(uint32_t moby, uint32_t* out_a, uint32_t* out_b, uint32_t* out_c);  // orig: 0x00250320
void moby_get_color_hi(uint32_t moby, uint32_t out_a, uint32_t out_b, uint32_t out_c);  // orig: 0x00250320
int32_t sub_00250a40(uint32_t bit, int32_t count);       // orig: 0x00250a40
int32_t rand_mod(int32_t n);                             // orig: 0x00257b50
int32_t rand_range(int32_t lo, int32_t hi);              // orig: 0x00257b90
float rand_range_f(float lo, float hi);                  // orig: 0x00257be8
float rand_angle();                                      // orig: 0x00257cb0
void sub_00259888_v(float f, uint32_t* dst, int32_t a, int32_t b, const Vec4& src);  // orig: 0x00259888 (src in $a3)
void sub_00259888(float f, uint32_t dst, int32_t a, int32_t b, uint32_t src);  // orig: 0x00259888 (src in $a3)
uint32_t sub_0025a420(uint32_t moby, uint32_t mask, int32_t clear);  // orig: 0x0025a420
float lerp_f(float a, float b, float t);                 // orig: 0x0025b6a8
uint32_t moby_pvar_word0(uint32_t moby);                 // orig: 0x0025c338
void moby_copy_hero_color(uint32_t moby);                // orig: 0x0025d1b8
int32_t moby_in_range_class_500(uint32_t moby);          // orig: 0x0025e3b8
void sub_0024f590(uint32_t moby, uint32_t idx, uint32_t node);  // orig: 0x0024f590
int32_t sub_00250aa0(int32_t n);                         // orig: 0x00250aa0
void moby_grid_move64(uint32_t moby, uint64_t rect64);  // orig: 0x00250b20
void moby_grid_move(uint32_t moby, uint32_t rect);       // orig: 0x00250b20
void moby_retire(uint32_t moby);                         // orig: 0x0024eb68
void moby_anim_advance(uint32_t m);                      // orig: 0x00250480
void moby_update_transform(uint32_t m);                  // orig: 0x00250df8
void moby_update_transform_kept(uint32_t m);             // orig: 0x00250f98
void region_release(uint32_t id);                        // orig: 0x0023edf0
void region_clear(uint32_t id);                          // orig: 0x0023e838
void remove_moby_5f3();                                  // orig: 0x002a9be0
void hero_head_look();                                   // orig: 0x002056a8
void hero_body_lean();                                   // orig: 0x00215b68
void hero_run_lean();                                    // orig: 0x00215340
void hero_aim_bones();                                   // orig: 0x00214fe8
void bone_node_update(uint32_t n);                       // orig: 0x00205168
void bone_nodes_update();                                // orig: 0x002054e8
int32_t path_push_out(uint32_t idx, uint32_t pos, uint32_t out, float r);  // orig: 0x00262030
void hero_path_push_out();                               // orig: 0x002347c0
void hero_platform_ride();                               // orig: 0x00233f80
void hero_rot_smooth(uint32_t obj, uint32_t src);       // orig: 0x0020f160
void hero_stick_move(int32_t mode, float scale);         // orig: 0x002118c8
void hero_smoke_fx();                                    // orig: 0x00209ec8
void hero_stick_speed();                                 // orig: 0x00211be8
int32_t hero_stick_quadrant();                           // orig: 0x00221df8
int32_t hero_stick_turn_frames();                        // orig: 0x00221d98
void hero_lock_heading(float range, float b, float c);   // orig: 0x00214ed8
void hero_rail_grab_check();                             // orig: 0x0020d330
void hero_rail_land_check();                             // orig: 0x0020cf58
void hero_head_motion();                                 // orig: 0x0020a540
void clank_glow_blink();                                 // orig: 0x002059d8
void hero_anim_advance();                                // orig: 0x00232670
void hero_vertical_velocity();                           // orig: 0x00214108
void hero_horizontal_velocity();                         // orig: 0x00214658
void hero_moby_sync();                                   // orig: 0x00208a10
void hero_moby_update();                                 // orig: 0x00208820
void hero_land_fx(int32_t puffs, int32_t sparks, int32_t ring);  // orig: 0x00209ca8
int32_t find_rail_v(const Vec4& pos, uint32_t* path_out, Vec4& point, int32_t* idx, float* dist, int32_t* loop_out, uint32_t skip, uint32_t only);  // orig: 0x0020cd08
int32_t find_rail(uint32_t pos, uint32_t path_out, uint32_t point, uint32_t idx, uint32_t dist, uint32_t loop_out, uint32_t skip, uint32_t only);  // orig: 0x0020cd08
int32_t find_rail_l3_v(const Vec4& pos, uint32_t* path_out, Vec4& point, int32_t* idx, float* dist, int32_t* loop_out, uint32_t skip, uint32_t only);  // orig: L3:0x00205830
int32_t path_closest_point_v(uint32_t path, const Vec4& p, Vec4& out, int32_t* idx, float* dist, int32_t loop, float range, float step, float h);  // orig: 0x0025df68
int32_t path_closest_point(uint32_t path, uint32_t p, uint32_t out, int32_t* idx, float* dist, int32_t loop, float range, float step, float h);  // orig: 0x0025df68
void spawn_particle_1c_v(const Vec4& pos, const Mat4* mat, float speed);  // orig: 0x0026de90
void spawn_particle_1c(uint32_t pos, uint32_t mat, float speed);  // orig: 0x0026de90
float surface_height_v(uint32_t pos, Vec4* n);           // orig: 0x00259e58
float surface_height(uint32_t pos, uint32_t n);          // orig: 0x00259e58
void moby_class_relocate(uint32_t cls, uint32_t tex, uint32_t remap, uint32_t idx);  // orig: 0x002406b0
void item_class_load(uint32_t cls, int32_t buf);                              // orig: 0x00245770
void moby_seq_relocate(uint32_t cls, uint32_t i);  // orig: 0x00240658
// camera system (camera.cpp)
int32_t camera_type_call(uint32_t fn, uint32_t cam, uint32_t other);       // orig: (helper) jalr to a camera type function
void camera_callback_call(uint32_t fn);                                      // orig: (helper) jalr to a queued camera callback
float camera_spring(float cur, float target, float k, float d, float max, uint32_t vel);  // orig: 0x001eb240
void camera_callbacks_run();                                                 // orig: 0x001eb188
void camera_marker_update(uint32_t cam);                                     // orig: 0x001eb468
void camera_type_init(uint32_t cam);                                         // orig: 0x001eb4c8
void camera_type_exit(uint32_t cam);                                         // orig: 0x001eb9d8
void camera_switch(uint32_t cam);                                            // orig: 0x001eb510
int32_t camera_accept(uint32_t cam, uint32_t cur);                           // orig: 0x001eb810
int32_t camera_select();                                                     // orig: 0x001eba20
void camera_angles_v(float* out, const Vec4& a, const Vec4& b, const Vec4& x, const Vec4& y, const Vec4& z);  // orig: 0x001ebb30
void camera_angles(uint32_t out, const Vec4& a, const Vec4& b, const Vec4& x, const Vec4& y, const Vec4& z);  // orig: 0x001ebb30
void camera_trans_hero_angles();                                             // orig: 0x001ebd10
void camera_trans_end_from_start();                                          // orig: 0x001ebdf0
void camera_trans_start_from_end();                                          // orig: 0x001ebe68
void camera_trans_begin(uint32_t old);                                       // orig: 0x001ebea0
int32_t camera_blend(uint32_t cam, uint32_t t);                              // orig: 0x001ec0f8
int32_t camera_orbit(uint32_t cam, uint32_t t);                              // orig: 0x001ec2d8
void camera_trans_step(uint32_t cam);                                        // orig: 0x001ec8b0
void camera_shake(uint32_t s, int32_t axis);                                 // orig: 0x001ec960
void camera_track();                                                         // orig: 0x001eca70
void camera_ground_check();                                                  // orig: 0x001ecdf0
void camera_masks();                                                         // orig: 0x001ecf40
void camera_timer_tick();                                                    // orig: 0x001ed060
void camera_update();                                                        // orig: 0x001ed0a8
int32_t camera_type_row(int32_t type);                                       // orig: 0x001ed230
void camera_select_first();                                                  // orig: 0x001ed280
void camera_init();                                                          // orig: 0x001ed358
void camera_fog(uint32_t pos);                                               // orig: 0x001ee6b8
float cos_ease(float a, float b, float t);                                   // orig: 0x00257e58
void mat_to_quat(Vec4& q, const Mat4& m);                                    // orig: 0x0025cef0
void mat_to_quat_at_v(Vec4& q, uint32_t m);                                  // orig: 0x0025d168
void mat_to_quat_at(uint32_t q, uint32_t m);                                 // orig: 0x0025d168
void quat_axis_angle(Vec4& q, const Vec4& axis, float angle);                // orig: 0x0025d1d0
void quat_rotate(Vec4& d, const Vec4& v, const Vec4& q);                     // orig: 0x0025fb78
void vec_rotate_axis(Vec4& d, const Vec4& v, const Vec4& axis, float angle); // orig: 0x0025fc08
uint32_t camera_marker_spawn(uint32_t pos);                                  // orig: 0x002a4de0

// the follow camera, type 0 (camera_follow.cpp)
void cam0_nop_a(uint32_t);                                                   // orig: 0x002e4160
void cam0_nop_b(uint32_t, uint32_t);                                         // orig: 0x002e4428
void cam0_nop_c(uint32_t);                                                   // orig: 0x002e5e30
int32_t cam0_accept(uint32_t, uint32_t);                                     // orig: 0x002e9c20
void cam0_exit(uint32_t);                                                    // orig: 0x002e9cb8
void cam0_hero_pos(uint32_t, uint32_t out);                                  // orig: 0x002e4168
int32_t cam0_target_v(Vec4& out, int32_t flag);                              // orig: 0x002e42c0
int32_t cam0_target(uint32_t out, int32_t flag);                             // orig: 0x002e42c0 (EE-address form)
void cam0_reset_target(uint32_t cam);                                        // orig: 0x002e4430
void cam0_place(int32_t reset);                                              // orig: 0x002e66b8
void cam0_from_transition(uint32_t cam);                                     // orig: 0x002e6bf8
void cam0_init(uint32_t cam);                                                // orig: 0x002e6d60
void cam0_follow_motion(uint32_t cam);                                       // orig: 0x002e46b8
void cam0_along_facing(Vec4& out, float h);                                  // orig: 0x002e48a0
float cam0_hero_angle(uint32_t cam, float h);                                // orig: 0x002e4580
float cubic_interp(float p0, float p1, float p2, float p3, float t);         // orig: 0x00257e20
void cam0_set_distance(int32_t both, float d, float rate);                   // orig: 0x002e8450
void cam0_set_height(float h, float rate);                                   // orig: 0x002e84b8
void cam0_set_look_height(int32_t rel, float h, float rate);                 // orig: 0x002e84f0
void cam0_set_mode(int32_t m);                                               // orig: 0x002e8568
void cam0_set_rates(float a, float b);                                       // orig: 0x002e8590
void cam0_set_flag226();                                                     // orig: 0x002e8620
void cam0_set_flag116();                                                     // orig: 0x002e8648
float cam0_turn_toward(uint32_t cam, const Vec4& dir, float limit);          // orig: 0x002e86b0
float cam0_turn_toward_point(uint32_t cam, uint32_t pos, float limit);       // orig: 0x002e88c8
void cam0_turn_dir(uint32_t dir, float speed, float limit);                  // orig: 0x002e8918
int32_t cam_mod_release(uint32_t cam);                                        // orig: 0x002e8388
int32_t cam_mod_claim(uint32_t cam);                                          // orig: 0x002e83c0
void cam0_set_22c(float v);                                                   // orig: 0x002e8490
void cam0_set_1cc(float v);                                                   // orig: 0x002e8540
void cam0_set_124(float a, float b);                                          // orig: 0x002e85d8
void cam0_set_flags3();                                                       // orig: 0x002e8680
int32_t point_in_cylinder(uint32_t pos, int32_t idx);                         // orig: 0x0025fa38
int32_t point_in_sphere(uint32_t pos, int32_t idx);                           // orig: 0x0025faf0
int32_t cam_mod_inside(uint32_t cam);                                         // orig: 0x002ec7e8
void cam_mod_update(uint32_t cam);                                            // orig: 0x002ec988
int32_t cam_mod5_accept(uint32_t cam);                                        // orig: 0x002ed230
int32_t cam_mod6_inside(uint32_t cam);                                        // orig: 0x002ed348
void cam_mod6_update(uint32_t cam);                                           // orig: 0x002ed498
int32_t cam_mod6_accept(uint32_t cam);                                        // orig: 0x002ed8b0
void cam_look_follow(uint32_t cam);                                            // orig: 0x002e9cc0
void cam_look_place(uint32_t cam);                                             // orig: 0x002e9dd0
void cam_look_reset(uint32_t cam);                                             // orig: 0x002e9ef0
void cam_look_update(uint32_t cam);                                            // orig: 0x002e9f38
int32_t cam_look_accept(uint32_t cam, uint32_t other);                         // orig: 0x002ea488
void cam_look_init(uint32_t cam);                                              // orig: 0x002ea7a0
void cam_look_exit(uint32_t cam);                                              // orig: 0x002ea810
void wind_update();                                                           // orig: 0x002a4890
void cam0_turn_point(uint32_t pos, float speed, float limit);                // orig: 0x002e8970
void cam0_nop_d(uint32_t);                                                   // orig: 0x002e8378
void cam0_hook_call(uint32_t fn);                                            // orig: (helper) jalr to *(0x167010)
void cam0_modes(uint32_t cam);                                               // orig: 0x002e6000
void cam0_target_kind(uint32_t cam);                                         // orig: 0x002e4930
void cam0_target_offset(uint32_t cam, Vec4& out);                            // orig: 0x002e4fe8
void cam0_spring_vec(uint32_t cam, uint32_t cur, const Vec4& target, float k, float d, float max);  // orig: 0x002e55b8
int32_t cam0_look_point(uint32_t cam, const Vec4& add, Vec4& out, uint32_t kind);  // orig: 0x002e5678
void cam0_look_update(uint32_t cam, const Vec4& add);                        // orig: 0x002e5730
void cam0_target_update(uint32_t cam);                                       // orig: 0x002e5830
int32_t cam_volume_contains(const Vec4& pos, uint32_t rec, float r);         // orig: 0x001ee318
uint32_t cam_volume_find(const Vec4& pos, float r);                          // orig: 0x001ee1b0
int32_t cam0_in_volume(const Vec4& pos);                                     // orig: 0x002e4290
int32_t cam0_volume_push(const Vec4& pos, float r);                          // orig: 0x002e4180
int32_t cam0_apply_probe(uint32_t cam, const Vec4& pos, int32_t hit_volume, int32_t hit_wall);  // orig: 0x002e7138
int32_t cam0_probe(uint32_t cam);                                            // orig: 0x002e7318
int32_t cam0_probe_none(uint32_t);                                           // orig: 0x002e7520
void cam0_slide_step(uint32_t cam, Vec4& out, Vec4& a, const Vec4& b, const Vec4& c, float dist, float back);  // orig: 0x002e7530
int32_t cam0_slide(uint32_t cam, Vec4& pos, float back, float r);            // orig: 0x002e7618
int32_t cam0_hit_behind(uint32_t cam, int32_t step, const Vec4& pos);        // orig: 0x002e79c8
int32_t cam0_walk_probe(uint32_t cam);                                       // orig: 0x002e7a30
void cam0_collide(uint32_t cam);                                             // orig: 0x002e7d20
void cam0_ease_offset(uint32_t cam);                                         // orig: 0x002e8f40
void cam0_volume_pass();                                                     // orig: 0x002e9aa0
float camera_angle_spring(float cur, float target, float k, float d, float max, uint32_t vel);  // orig: 0x001eb328
void cam0_backoff(uint32_t cam, float& k, float& d);                         // orig: 0x002e9518
void cam0_position(uint32_t cam);                                            // orig: 0x002e9720
void cam0_stick(uint32_t cam, float& a, float& b);                           // orig: 0x002e89b0
void cam0_turn(uint32_t cam, float a, float b);                              // orig: 0x002e8bb8
void cam0_aim(uint32_t cam);                                                 // orig: 0x002e9010
void cam0_place_frame(uint32_t cam);                                         // orig: 0x002e9bb0
void cam0_frame_end(uint32_t cam);                                           // orig: 0x002e5e38
void cam0_update(uint32_t cam);                                              // orig: 0x002e9c28
int32_t moby_coll_flag8(uint32_t m);                                         // orig: 0x00259830
void moby_hit_record_v(uint32_t m, const uint8_t* rec);                      // orig: 0x00259a88
void moby_hit_record(uint32_t m, uint32_t rec);                              // orig: 0x00259a88 (EE-address form)

// the overlay's start-up (overlay_init.cpp)
void sub_00264bf8();                                                         // orig: 0x00264bf8
void sub_002a07c8();                                                         // orig: 0x002a07c8
void sub_002a1128();                                                         // orig: 0x002a1128
void particle_table_init();                                                      // orig: 0x00268900
void level_arrival_message(int32_t level);                                   // orig: 0x00262d38
void level_mark_visited(int32_t l);                                          // orig: 0x002607d0
void quick_select_add_level_item(int32_t l);                                 // orig: 0x0023d3d8
void item_give(int32_t id, int32_t equip);                                   // orig: 0x00260860
void inventory_level_start();                                                // orig: 0x0023d198
void overlay_init_tables();                                                  // orig: 0x0023d028

// the gameplay frame (game_frame.cpp)
void prof_mark(uint32_t, int32_t);                                           // orig: 0x001f6d98
void debug_view_2();                                                         // orig: 0x001f6bf8
void debug_view_3();                                                         // orig: 0x001f6c00
void debug_view_4();                                                         // orig: 0x001f6c08
void debug_view_5();                                                         // orig: 0x001f6c10
void debug_view_6();                                                         // orig: 0x001f6c18
void gameplay_frame();                                                       // orig: 0x00297f78

// level setup (level_setup.cpp)
void sub_0023f0d8(uint32_t p);                                              // orig: 0x0023f0d8
void sub_0023f120(uint32_t p);                                              // orig: 0x0023f120
void sub_0023f630(uint32_t p, int32_t base, uint32_t list, int32_t n);      // orig: 0x0023f630
void sub_0023f768(uint32_t p, int32_t n);                                   // orig: 0x0023f768
void class_update_fn_set(int32_t cls);                                      // orig: 0x00264688
void moby_class_register(uint32_t hdr, uint32_t tex, uint32_t remap, uint32_t cls);  // orig: 0x002409b8
void level_collision_set(uint32_t p);                                       // orig: 0x00241728
void sub_00241938(uint32_t p, int32_t n);                                   // orig: 0x00241938
void sub_00284e00();                                                        // orig: 0x00284e00
uint32_t moby_find_uid(int32_t unused, int32_t uid);                          // orig: 0x002851c0
void moby_data_restore(uint32_t list);                                        // orig: 0x00285208
void checkpoint_mobys_restore();                                              // orig: 0x002852c0
void checkpoint_restore();                                                    // orig: 0x00285008
void tie_class_register(uint32_t hdr, uint32_t tex, uint32_t remap, uint32_t cls);  // orig: 0x00240aa8
void shrub_class_register(uint32_t hdr, uint32_t tex, uint32_t remap, uint32_t def, uint32_t cls);  // orig: 0x00240e80
void lod_bands_set();                                                       // orig: 0x00293f00
void moby_lod_set(uint32_t p, uint32_t tex);                                // orig: 0x00241458
void sub_00241788(uint32_t p, int32_t n);                                   // orig: 0x00241788
int32_t wad_decompress(uint32_t src, uint32_t dst);                         // orig: 0x0024d8d0
void frame_rate_set(int32_t pal);                                           // orig: 0x00261358
void fog_setup();                                                           // orig: 0x001f7170
void bg_color_set(uint32_t r, uint32_t g, uint32_t b);                      // orig: 0x00200ef0
int32_t sub_00284d90(uint32_t idx, int32_t n);                              // orig: 0x00284d90
void sub_001f83d0();                                                        // orig: 0x001f83d0
void sub_001fb028();                                                        // orig: 0x001fb028
void level_slot_set(uint32_t v, int32_t i);                                 // orig: 0x0024a798
void sub_00266d48();                                                        // orig: 0x00266d48
void sub_0028df58(uint32_t p);                                              // orig: 0x0028df58
void ship_hide();                                                           // orig: 0x0028e990
void ship_init(uint32_t m);                                                 // orig: 0x0028e8a0
void hero_level_reset();                                                    // orig: 0x00204f80
uint32_t gameplay_load(int32_t level);                                      // orig: 0x00241940
void memory_layout();                                                       // orig: 0x0023d520
void sub_0023f848(uint32_t p);                                              // orig: 0x0023f848
uint32_t armor_load(uint32_t dst, uint32_t core);                           // orig: 0x0023fa00
void gs_ram_upload(uint32_t data, int32_t n, uint32_t list);               // orig: 0x00240498
void moby_update_pass();                                                    // orig: 0x00264740
void level_setup_body(int32_t full, int32_t restore);                       // orig: 0x00244110 (up to its moby pass)
int32_t level_setup(int32_t full, int32_t restore);                         // orig: 0x00244110
// render setup (render_setup.cpp)
void screen_setup();                                                        // orig: 0x001f7848
void projection_setup();                                                    // orig: 0x001f7980
void hero_release_attached();                            // orig: 0x00228fa8
void retire_children(uint32_t obj);                      // orig: 0x00262840
void trail_update(uint32_t obj, int32_t fade);           // orig: 0x00262608
uint32_t spawn_moby_1df_v(uint32_t val, const Vec4& pos);  // orig: 0x002ce1f0
uint32_t spawn_moby_1df(uint32_t val, uint32_t pos);     // orig: 0x002ce1f0
uint32_t spawn_moby_307_v(const Vec4& pos, float scale); // orig: 0x002d7e90
uint32_t spawn_moby_307(uint32_t pos, float scale);      // orig: 0x002d7e90
void use_agents(uint32_t prev);                          // orig: 0x002cdcc0
void item_list_build(int32_t flagged_only);              // orig: 0x0029a400
float rand_range_signed_f(float lo, float hi);           // orig: 0x00257c48
float approach_f_v(float& p, float target, float step);   // orig: 0x0025b868
float approach_f(uint32_t p, float target, float step);  // orig: 0x0025b868
int32_t step_index(uint32_t count, int32_t cur, int32_t delta, int32_t wrap);  // orig: 0x0025d7a0
float sub_0025da70_v(const Vec4& a, const Vec4& b, float h);  // orig: 0x0025da70
float sub_0025da70(uint32_t a, uint32_t b, float h);     // orig: 0x0025da70
void vec3_clamp_length_v(Vec4& v, float max);            // orig: 0x0025f730
void vec3_clamp_length(uint32_t v, float max);           // orig: 0x0025f730
void sub_0025f780(uint32_t p, uint32_t list, int32_t max);  // orig: 0x0025f780
void sub_0024f608(uint32_t moby, uint32_t node);         // orig: 0x0024f608
void moby_palette_colour_1(uint32_t moby, uint32_t out); // orig: 0x00250350
void moby_palette_colour_1_v(uint32_t moby, Vec4& out);  // orig: 0x00250350
void moby_palette_colour_0(uint32_t moby, uint32_t out); // orig: 0x002503c0
void moby_palette_colour_0_v(uint32_t moby, Vec4& out);  // orig: 0x002503c0
int32_t hero_wall_probe(float* dist, float z, float reach);    // orig: 0x0020a240
int32_t hero_wall_probe(uint32_t dist, float z, float reach);  // orig: 0x0020a240
void hero_collide_resolve();                                 // orig: 0x002137a8
int32_t hero_collide_push(int32_t mode);                       // orig: 0x002133a8
void hero_ground_update();                                   // orig: 0x002127b8
void hero_move_integrate();                                  // orig: 0x00213880
void hero_slide_move();                                      // orig: 0x002167d0
void hero_wall_slide(int32_t facing);                        // orig: 0x00212318
void hero_edge_brake(float k, float min_len);                 // orig: 0x00217118
void hero_shadow_update();                                   // orig: 0x00208b60
int32_t hero_ledge_check_v(const Vec4& pos, const float* ref_yaw);  // orig: 0x0020c758
int32_t hero_ledge_check(uint32_t pos, const float* ref_yaw);  // orig: 0x0020c758
void hero_ledge_grab_check();                                // orig: 0x0020bed0
void hero_air_wall_check();                                  // orig: 0x0020b5b8
void hero_look_at_update();                                  // orig: 0x0020ac98
void hero_footsteps_update();                                // orig: 0x00206080
void hero_wall_lean();                                       // orig: 0x0020a318
void hero_footstep(int32_t a, uint32_t foot, int32_t sound);   // orig: 0x00205f60
int32_t hero_idle_event_pick(int32_t* out);                    // orig: 0x00229010
void hero_pick_target_323();                                 // orig: 0x00221ee0
int32_t hero_pick_target_2f6();                              // orig: 0x00222158
void hero_motion_update();                                   // orig: 0x00221310
int32_t hero_floor_ahead();                                   // orig: 0x00229910
int32_t moby_sphere_contact(uint32_t moby, uint32_t pos, int32_t b, uint32_t a, uint32_t c, uint32_t flags, float radius, float f, float speed);  // orig: 0x002598b0
int32_t moby_sphere_contact_v(uint32_t moby, const Vec4& pos, int32_t b, uint32_t a, uint32_t c, uint32_t flags, float radius, float f, float speed);  // orig: 0x002598b0
float floor_probe_v(float up, const Vec4& pos, uint32_t flags);  // orig: 0x00259698
float floor_probe(float up, uint32_t pos, uint32_t flags);  // orig: 0x00259698
float sub_00259468(uint32_t pos, uint32_t vel, int32_t* frames_out, float g);  // orig: 0x00259468
float sub_00259468_v(const Vec4& pos, const Vec4& vel, int32_t* frames_out, float g);  // orig: 0x00259468
void sub_00259508_v(Vec4& dst, const Vec4& base, const Vec4& vel, int32_t n, float g);  // orig: 0x00259508
void sub_00259508(uint32_t dst, uint32_t base, uint32_t vel, int32_t n, float g);  // orig: 0x00259508
void quat_from_euler_v(Vec4& dst, const Vec4& angles);  // orig: 0x00259f50
void quat_from_euler(uint32_t dst, uint32_t angles);     // orig: 0x00259f50
void vec4_lerp_v(Vec4& dst, const Vec4& a, const Vec4& b, float t);  // orig: 0x0025b6b8
void vec4_lerp(uint32_t dst, uint32_t a, uint32_t b, float t);  // orig: 0x0025b6b8
uint32_t rgba_pack(float r, float g, float b, float a);  // orig: 0x0025bfe0
void sub_0024f0e8(uint32_t moby, uint32_t slot);         // orig: 0x0024f0e8
void sub_0024ebc0(uint32_t m);                           // orig: 0x0024ebc0
uint32_t sub_0024f028(uint32_t moby, uint32_t idx);      // orig: 0x0024f028
float sub_0025abf0(uint32_t a, uint32_t b, float* t_out, float speed, float g);  // orig: 0x0025abf0
float sub_0025bb38(int32_t dir, float a, float b);       // orig: 0x0025bb38
void mat3_normalize_columns(uint32_t m);                 // orig: 0x0025c170
void sub_0024eed8_v(uint32_t m, int32_t idx, const Vec4& pos, float a, float b);  // orig: 0x0024eed8
void sub_0024eed8(uint32_t m, int32_t idx, uint32_t pos, float a, float b);  // orig: 0x0024eed8
void moby_set_anim(uint32_t m, uint32_t set, int32_t frame);  // orig: 0x002577c8
int32_t solve_quadratic(float* r_hi, float* r_lo, float a, float b, float c);  // orig: 0x002595a0
float spring_step(uint32_t p, float t, float k, float d, float vmax);  // orig: 0x0025b798
float turn_towards(uint32_t ang, int32_t dir, float target, float step);  // orig: 0x0025bc00
int32_t point_in_box(uint32_t pos, int32_t idx);         // orig: 0x0025f960
void sub_0024a7e8(float* x, float* y, int32_t idx, float sx, float sy);  // orig: 0x0024a7e8
float moby_anim_time(uint32_t m);                        // orig: 0x0024edc8
float spring_to_v(float& p, uint32_t v, float target, float k, float d, float vmax);  // orig: 0x0025b8c0
float spring_to(uint32_t p, uint32_t v, float target, float k, float d, float vmax);  // orig: 0x0025b8c0
void sub_0025d238_v(const Vec4& src, Vec4& dst);         // orig: 0x0025d238
void sub_0025d238(uint32_t src, uint32_t dst);           // orig: 0x0025d238
void rle4_decode(uint32_t dst, int32_t idx, uint32_t base, uint32_t table);  // orig: 0x002483c8
uint32_t grid_find(uint32_t pos, float* dist2);          // orig: 0x00256f14
int32_t sprite_cache_get(uint32_t key);                  // orig: 0x00249d80
int32_t map_side(int32_t x, int32_t y, int32_t ax, int32_t ay, int32_t bx, int32_t by);  // orig: 0x0024abf8
int32_t map_condition(uint32_t fn, int32_t x, int32_t y, float fx, float fy, float fz);  // orig: 0x00248bf0..
void map_reveal(uint32_t pos);                            // orig: 0x002484e0
void moby_pose_blend(uint32_t m);                         // orig: 0x00252990
void moby_anim_step(uint32_t s);                          // orig: 0x0024f190
uint32_t moby_pose_matrices(uint32_t m, int32_t n, int32_t out, uint32_t t8 = 0, uint32_t t9 = 0);  // orig: 0x002531e0 (returns $a3)
void moby_pose_capture(uint32_t m, uint32_t flags);       // orig: 0x00251ce8
void moby_anim_set(uint32_t m, uint32_t set, int32_t frame, int32_t frames);  // orig: 0x00257880
void moby_pose_eval(uint32_t m);                          // orig: 0x00254b58
void moby_pose_encode(uint32_t list, uint32_t out, uint32_t cls, int32_t pad, uint32_t moby);  // orig: 0x002092c8
void moby_pose_matrices_masked(uint32_t m, uint32_t mask);  // orig: 0x00254108
void moby_chain_matrices(uint32_t m, int32_t k, uint32_t chains, uint32_t dst);  // orig: 0x00253fa0
void moby_chain_world(uint32_t m, int32_t k, uint32_t chains, uint32_t dst);  // orig: 0x0024f8f0
void moby_anim_rec_start(uint32_t m, uint32_t r, uint32_t set, uint32_t frame, int32_t frames, int32_t step);  // orig: 0x0024f3e0
void moby_chain_world_one_v(uint32_t m, uint32_t id, Mat4& d);  // orig: 0x0024f728
void moby_chain_world_one(uint32_t m, uint32_t id, uint32_t dst);  // orig: 0x0024f728
void moby_chain_point_v(uint32_t m, uint32_t id, Vec4& out);  // orig: 0x0024f7c8
void moby_chain_point(uint32_t m, uint32_t id, uint32_t out);  // orig: 0x0024f7c8
void hero_chain_world(uint32_t moby);                     // orig: 0x00209240
void hero_smoke_puffs(int32_t n, int32_t mode);           // orig: 0x00209a40
void hero_hold_release();                                 // orig: 0x0020e698
void hero_hold_start();                                   // orig: 0x0020e4c8
void hero_hold_update();                                  // orig: 0x0020e728
void hero_look_fx();                                      // orig: 0x0020b1d8
void hero_gadget_glow();                                  // orig: 0x002080b0
void hero_slot_update(int32_t k, uint32_t prev = 0);     // orig: 0x0020fea0
void hero_slot_mobys_update();                           // orig: 0x0020f580
void hero_layers_tick();                                 // orig: 0x0020ea50
void hero_slot_close(int32_t k, int32_t delay);          // orig: 0x0020fca8
void hero_slots_update(uint32_t prev = 0);               // orig: 0x00210748
void hero_rider_attach();                                 // orig: 0x002061f0
void sub_00240348(uint32_t dst, int64_t a, int64_t b, int64_t c, int64_t d, int64_t idx);  // orig: 0x00240348
void sub_002400f0(uint32_t dst, uint32_t tex, int64_t a, int64_t b, int64_t c, int64_t d, int64_t idx);  // orig: 0x002400f0
void sub_0024f440(uint32_t m, uint32_t set, uint32_t frame, uint32_t dst);  // orig: 0x0024f440
void moby_init(uint32_t m, uint32_t cls);                // orig: 0x0024e930
uint32_t moby_spawn(uint32_t cls);                       // orig: 0x0024e838
void moby_update_sound(uint32_t m);                      // orig: 0x0024ec80
int32_t path_advance_v(uint32_t path, Vec4& out, int32_t* idx, float* frac, int32_t loop, float step); // orig: 0x0025d808
int32_t path_advance(uint32_t path, uint32_t out, int32_t* idx, float* frac, int32_t loop, float step);  // orig: 0x0025d808
float spring_angle_v(float& ang, uint32_t vel, int32_t dir, float target, float k, float d, float vmax);  // orig: 0x0025bc98
float spring_angle(uint32_t ang, uint32_t vel, int32_t dir, float target, float k, float d, float vmax);  // orig: 0x0025bc98
float closest_point_on_segment_xy_v(Vec4& out, const Vec4& p, uint32_t a, uint32_t b); // orig: 0x0025db00
float closest_point_on_segment_xy(uint32_t out, uint32_t p, uint32_t a, uint32_t b);  // orig: 0x0025db00
float closest_point_on_segment_r(Vec4& out, const Vec4& p, const Vec4& a, const Vec4& b, float h); // orig: 0x0025dcd8
float closest_point_on_segment_v(Vec4& out, const Vec4& p, uint32_t a, uint32_t b, float h); // orig: 0x0025dcd8
float closest_point_on_segment(uint32_t out, uint32_t p, uint32_t a, uint32_t b, float h);  // orig: 0x0025dcd8
uint32_t moby_update_light(uint32_t m);                  // orig: 0x00257024
int32_t sub_00256de0(uint32_t pos, float* t, int32_t* idx);  // orig: 0x00256de0
void sub_00259bc8(uint32_t moby, uint32_t a, uint32_t b, uint32_t pos, uint32_t dir, float strength);  // orig: 0x00259bc8
void sub_00259bc8_v(uint32_t moby, uint32_t a, uint32_t b, uint32_t pos, const Vec4& dir, float strength);  // orig: 0x00259bc8
void mat_to_euler_r(const Mat4& m, Vec4& out);           // orig: 0x0025d330
void mat_to_euler_v(const Mat4& m, uint32_t out);        // orig: 0x0025d330
void mat_to_euler(uint32_t m, uint32_t out);             // orig: 0x0025d330

// ---- 0x0026xxxx -------------------------------------------------------------
void sub_00262500(uint32_t v, uint32_t obj);             // orig: 0x00262500
uint32_t moby_pvar_word2(uint32_t moby);                 // orig: 0x002603d0
void sub_00265828(int32_t flag);                         // orig: 0x00265828
void music_pause_timer(int32_t t);                       // orig: 0x00265880
float pad_stick_history(int32_t back);                   // orig: 0x00266e80

// ---- 0x0028xxxx / 0x002axxxx -------------------------------------------------
int32_t sub_0028d8c0(uint32_t v, int32_t slot);          // orig: 0x0028d8c0
void sub_0028d918(int32_t slot);                         // orig: 0x0028d918
void sub_002a07f8();                                     // orig: 0x002a07f8
void sub_00262b00(uint32_t obj, uint32_t ang, uint32_t off, float scale, float delta);  // orig: 0x00262b00
void sub_002a0858();                                     // orig: 0x002a0858
void vec_from_spherical_v(Vec4& d, float r, float a, float b);  // orig: 0x00262c50
void vec_from_spherical(uint32_t dst, float r, float a, float b);
uint32_t auto_aim_target(uint32_t ignore, const Vec4& from, const Vec4& dir, float* yaw, float* pitch, float* height, float cone);  // orig: 0x002630c8
uint32_t spawn_projectile_100(uint32_t owner, uint32_t pos, uint32_t target, float aim_yaw, float aim_pitch, float aim_h, float ry, float rx);  // orig: 0x002a96f8
uint32_t spawn_projectile_100_v(uint32_t owner, const Vec4& pos, uint32_t target, float aim_yaw, float aim_pitch, float aim_h, float ry, float rx);  // orig: 0x002a96f8
uint32_t auto_aim_target(uint32_t ignore, uint32_t from, uint32_t dir, uint32_t yaw, uint32_t pitch, uint32_t height, float cone);  // orig: 0x002630c8  // orig: 0x00262c50
void sub_00262cf0(uint32_t dst, uint32_t base, float r, float a, float b);  // orig: 0x00262cf0
void sub_00263d40(int32_t v, int32_t frames);            // orig: 0x00263d40
int32_t pad_buttons_together(uint32_t mask_a, uint32_t mask_b, int32_t back, int32_t max_gap);  // orig: 0x00266e00
void sub_00262b80(uint32_t dst, uint32_t a, uint32_t b, float r, float da, float db);  // orig: 0x00262b80
void sub_002a0ba8(uint32_t reg, uint64_t d);             // orig: 0x002a0ba8
uint32_t alloc_particle(uint32_t kind);                  // orig: 0x002678b8
void free_particle(uint32_t p);                                   // orig: 0x00267a08
uint32_t spawn_particle_3b_v(const Vec4& pos, uint32_t color, uint32_t frame, uint32_t tbl, uint32_t alt, uint32_t life, uint32_t rel, float size);  // orig: 0x00272f68
uint32_t spawn_particle_3b(uint32_t pos, uint32_t color, uint32_t frame, uint32_t tbl, uint32_t alt, uint32_t life, uint32_t rel, float size);  // orig: 0x00272f68
uint32_t spawn_particle_3c_v(const Vec4& pos, const Vec4& vel, uint32_t color, uint32_t life, uint32_t frame, uint32_t rel, float size);  // orig: 0x002730e0
uint32_t spawn_particle_3c(uint32_t pos, uint32_t vel, uint32_t color, uint32_t life, uint32_t frame, uint32_t rel, float size);  // orig: 0x002730e0
uint32_t spawn_particle_3e_v(const Vec4& pos, int32_t life, uint32_t kind);  // orig: 0x00273708
uint32_t spawn_particle_3e(uint32_t pos, int32_t life, uint32_t kind);      // orig: 0x00273708
uint32_t spawn_particle_2f(uint32_t pos, uint32_t vel, float f);  // orig: 0x00271b18
uint32_t spawn_particle_2f_v(const Vec4& pos, const Vec4& vel, float f);  // orig: 0x00271b18
uint32_t spawn_particle_46_v(const Vec4& pos, uint32_t idx, float f);  // orig: 0x00274cf8
uint32_t spawn_particle_46(uint32_t pos, uint32_t idx, float f);  // orig: 0x00274cf8
int32_t sub_0028c8b0(uint32_t obj, float x, float lo, float hi);  // orig: 0x0028c8b0
int32_t sub_0028c9a0(uint32_t obj, uint32_t pos);        // orig: 0x0028c9a0
int32_t sound_start(uint32_t snd, uint32_t flags, uint32_t moby, uint32_t pos, int32_t volume);  // orig: 0x0028d970
int32_t moby_sound_start(int32_t idx, uint32_t flags, uint32_t moby);  // orig: 0x0028dbe8
int32_t level_sound_start(int32_t a, int32_t b, int32_t c, uint32_t flags, uint32_t moby);  // orig: 0x0028de68
void sub_00262528(uint32_t obj, uint32_t b, int32_t c);  // orig: 0x00262528
uint32_t spawn_moby_593(uint32_t unused, uint32_t pos, uint32_t v, float scale, float a, float b);  // orig: 0x002a99b0
uint32_t spawn_moby_593_v(uint32_t unused, const Vec4& pos, uint32_t v, float scale, float a, float b);  // orig: 0x002a99b0
uint32_t spawn_moby_5f3_v(uint32_t unused, const Vec4& pos);  // orig: 0x002a9aa0
uint32_t spawn_moby_5f3(uint32_t unused, uint32_t pos);  // orig: 0x002a9aa0
uint32_t spawn_moby_a7(uint32_t v);                      // orig: 0x002b4138
void place_moby_5f3(uint32_t unused, uint32_t pos);      // orig: 0x002a9b80
void place_moby_5f3_v(uint32_t unused, const Vec4& pos);  // orig: 0x002a9b80
void spawn_particle_4_v(const Vec4& pos, const Vec4& vel, uint32_t colour, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                        int32_t additive);  // orig: 0x00269958
void spawn_particle_4(uint32_t pos, uint32_t vel, uint32_t colour, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                      int32_t additive);                 // orig: 0x00269958
void spawn_particle_23_v(const Vec4& pos, const Vec4& vel, int32_t kind, uint32_t a);  // orig: 0x0026f548
void spawn_particle_23(uint32_t pos, uint32_t vel, int32_t kind, uint32_t a);  // orig: 0x0026f548
uint32_t spawn_particle_22_v(const Vec4& pos, const Vec4& vel, float scale, float h);  // orig: 0x0026f080
uint32_t spawn_particle_22(uint32_t pos, uint32_t vel, float scale, float h);  // orig: 0x0026f080
uint32_t spawn_particle_2e_v(const Vec4& pos, const Vec4& vel, uint32_t a, float size, float b);  // orig: 0x002718d0
uint32_t spawn_particle_2e(uint32_t pos, uint32_t vel, uint32_t a, float size, float b);  // orig: 0x002718d0
void spawn_particle_19_v(const Vec4& pos, const Vec4& vel, int32_t flag);  // orig: 0x0026d598
void spawn_particle_19(uint32_t pos, uint32_t vel, int32_t flag);  // orig: 0x0026d598
uint32_t spawn_particle_2d_v(const Vec4& pos, uint32_t a, uint32_t colour, float size, float b);  // orig: 0x002715e8
uint32_t spawn_particle_2d(uint32_t pos, uint32_t a, uint32_t colour, float size, float b);  // orig: 0x002715e8
int32_t sub_00260400_v(uint32_t unused, uint32_t moby, Vec4& pos, Vec4& ang, Vec4& out_pos, Vec4& out_ang);  // orig: 0x00260400
int32_t sub_00260400(uint32_t unused, uint32_t moby, uint32_t pos, uint32_t ang, uint32_t out_pos, uint32_t out_ang);  // orig: 0x00260400
int32_t sub_00260668_v(uint32_t unused, uint32_t moby, Vec4& pos, Vec4& ang, Vec4& out_pos, Vec4& out_ang);  // orig: 0x00260668
int32_t sub_00260668(uint32_t unused, uint32_t moby, uint32_t pos, uint32_t ang, uint32_t out_pos, uint32_t out_ang);  // orig: 0x00260668
void attach_frame_set_v(uint32_t out, const Vec4& pos, const Vec4& base_ang, const Vec4& ang);  // orig: 0x00260738
void attach_frame_set(uint32_t out, uint32_t pos, uint32_t base_ang, uint32_t ang);  // orig: 0x00260738
int32_t sub_002604f0_v(uint32_t obj, uint32_t moby, Vec4& pos, Vec4& ang, Vec4& out_pos, Vec4& out_ang);  // orig: 0x002604f0
int32_t sub_002604f0(uint32_t obj, uint32_t moby, uint32_t pos, uint32_t ang, uint32_t out_pos, uint32_t out_ang);  // orig: 0x002604f0
uint32_t spawn_particle_35_v(const Vec4& pos, uint32_t a, uint32_t colour, int32_t mode, uint32_t b, const Vec4& vec, float s0,
                             float s1, float c);  // orig: 0x00272060
uint32_t spawn_particle_35(uint32_t pos, uint32_t a, uint32_t colour, int32_t mode, uint32_t b, uint32_t vec, float s0,
                           float s1, float c);  // orig: 0x00272060
int32_t sphere_mesh_collide(uint32_t pos, float radius);  // orig: 0x001f3170
int32_t mobys_in_sphere_v(const Vec4& pos, float radius, uint32_t flags, uint32_t ignore, const uint32_t* rec);  // orig: 0x001f2868
int32_t mobys_in_sphere(uint32_t pos, float radius, uint32_t flags, uint32_t ignore, uint32_t rec);  // orig: 0x001f2868
int32_t sphere_collide_v(const Vec4& pos, float radius, uint32_t flags, uint32_t ignore);  // orig: 0x001f0d60
int32_t sphere_collide(uint32_t pos, float radius, uint32_t flags, uint32_t ignore);  // orig: 0x001f0d60
int32_t capsule_collide_v(const Vec4& pos, float radius, float height, uint32_t flags, uint32_t ignore);  // orig: 0x001f19a0
int32_t capsule_collide(uint32_t pos, float radius, float height, uint32_t flags, uint32_t ignore);  // orig: 0x001f19a0
void collide_vu_reset();                                  // orig: (host) test support
int32_t segment_collide_v(const Vec4& start, const Vec4& end, uint32_t flags, uint32_t ignore, const uint32_t* rec);  // orig: 0x001efc70
int32_t segment_collide(uint32_t start, uint32_t end, uint32_t flags, uint32_t ignore, uint32_t rec);  // orig: 0x001efc70
int32_t region_find(float x, float y, float z);          // orig: 0x002a3cd8
int32_t region_height_sample_v(float* h, Vec4* n, float x, float y, float z);  // orig: 0x002a4530
int32_t region_height_sample(uint32_t h, uint32_t n, float x, float y, float z);  // orig: 0x002a4530
void region_height_splash(uint32_t regs, int32_t n, int32_t add, float x, float y, float radius, float amount);  // orig: 0x002a3ec8
void spark_pairs_v(Vec4& pos, int32_t n);                 // orig: 0x002a7e20
void spark_pairs(uint32_t pos, int32_t n);               // orig: 0x002a7e20
uint32_t pad_button_history(uint32_t mask, int32_t back, int32_t* ago);  // orig: 0x00266d60

// ---- 0x002exxxx -------------------------------------------------------------
void uid5_set_pvar_80(uint32_t src);                     // orig: 0x002ea9d8
void uid5_set_pvar_90(uint32_t src);                     // orig: 0x002eaa30


// overlay entry and main loop (overlay_entry.cpp)
// The host's libpad: the packet the pad returns this frame (2 button bytes active low, then the
// right x / y and left x / y sticks), filled by the host before each frame.
extern uint8_t g_host_pad_packet[6];
void prof_mark_end(uint32_t, int32_t);                                       // orig: 0x001f6da0
void debug_hook_1f6d90();                                                    // orig: 0x001f6d90
void frame_counters_reset();                                                 // orig: 0x001f6d70
void music_volumes_default();                                                // orig: 0x00265860
void autosave_tick();                                                        // orig: 0x00284c50
void render_enables_init();                                                  // orig: 0x001f3638
void debug_strings_flush();                                                  // orig: 0x001f36f8
void packet_call_151b60();                                                   // orig: 0x00201350
void packet_call_display();                                                  // orig: 0x00200f40
void start_mode_select();                                                    // orig: 0x0028eed0
uint32_t zone_grid_cell(int32_t x, int32_t y, int32_t z);                     // orig: 0x001f7278
uint32_t zone_grid_neighbour(float frac, int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1);  // orig: 0x001f7350
void zones_from_camera();                                                    // orig: 0x001f7408
void zones_update();                                                         // orig: 0x001f77f8
uint32_t moby_visible_queue();                                               // orig: 0x00250768
void moby_update_visible();                                                  // orig: 0x00264848
void moby_updates();                                                         // orig: 0x002648e0
void draw_callback_add(uint32_t fn, uint32_t arg);                           // orig: 0x001f9598
void moby_607_update(uint32_t m);                                            // orig: 0x002d37d0
void moby_530_update(uint32_t m);                                            // orig: 0x002d1e80
void moby_plant_update(uint32_t m);                                          // orig: 0x002e3ca8
void moby_1060_light_mark(uint32_t m);                                       // orig: 0x002df4a8
void moby_1060_update(uint32_t m);                                           // orig: 0x002df4f8
void moby_809_update(uint32_t m);                                            // orig: 0x002a4cd8
void path_point(float t, uint32_t path, int32_t loop, uint32_t pos, uint32_t rot, uint32_t flags);  // orig: 0x00262e40
void moby_1564_update(uint32_t m);                                           // orig: 0x002e3a88
void moby_1413_update(uint32_t m);                                           // orig: 0x002e0988
int32_t countdown_u8(uint32_t p);                                            // orig: 0x001fefc8
int64_t point_on_screen_v(const Vec4& pt, float r);                           // orig: 0x002002e0
int64_t point_on_screen(uint32_t pt, float r);                               // orig: 0x002002e0
int32_t group_first_r(uint32_t& out, int32_t group, int32_t a, int32_t b);   // orig: 0x002591d0
int32_t group_first(uint32_t out, int32_t group, int32_t a, int32_t b);      // orig: 0x002591d0
int32_t group_next_r(uint32_t& out, uint32_t cur, int32_t a, int32_t b);     // orig: 0x002592b8
int32_t group_next(uint32_t out, uint32_t cur, int32_t a, int32_t b);        // orig: 0x002592b8
uint32_t crate_spawn_companion(uint32_t m);                                  // orig: 0x002d8138
uint32_t moby_hit_state(uint32_t m);                                         // orig: 0x0025c368
int32_t moby_hit_react_r(uint32_t m, uint32_t hit, uint32_t hs, uint32_t mask, int32_t& state, float* dmg, int32_t* weapon, int32_t kind);  // orig: 0x0025a478
int32_t moby_hit_react(uint32_t m, uint32_t hit, uint32_t hs, uint32_t mask, uint32_t state, uint32_t dmg, uint32_t weapon, int32_t kind);  // orig: 0x0025a478
int32_t path_crossing_v(int32_t path, const Vec4& from, const Vec4& to, Vec4& out);  // orig: 0x00261b48
int32_t path_crossing(int32_t path, uint32_t from, uint32_t to, uint32_t out);  // orig: 0x00261b48
uint32_t pickup_spawn(uint32_t pos, uint32_t item, int32_t count, int32_t n);  // orig: 0x002c52a8
int32_t crate_ammo_kind(uint32_t m, uint32_t kind);                          // orig: 0x00257218
int32_t crate_ammo_kind_r(uint32_t m, int32_t& kind);                        // orig: 0x00257218
int32_t bolt_fly_to(uint32_t m, uint32_t target, float yaw, float pitch);    // orig: 0x002a7210
uint32_t bolt_spawn_v(uint32_t src, const Vec4& pos, const Vec4& dir, uint32_t flags, int32_t value, uint32_t tag);  // orig: 0x002a7438
uint32_t bolt_spawn(uint32_t src, uint32_t pos, uint32_t dir, uint32_t flags, int32_t value, uint32_t tag);  // orig: 0x002a7438
void bolts_drop(uint32_t m, int32_t lo, int32_t hi, uint32_t flags, int32_t path);  // orig: 0x00260a88
void moby_drop_bolts(uint32_t m, uint32_t flags, int32_t path);               // orig: 0x00257470
void crate_break_drops(uint32_t m);                                          // orig: 0x002d0538
uint32_t shard_spawn_v(uint32_t w10, const Vec4& pos, const Vec4& v, uint32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t grp, float scale);  // orig: 0x002ac910
uint32_t chunk_spawn_v(uint32_t src, const Vec4& vel, const Vec4& pos, const Vec4& rot, uint32_t cls, uint32_t tag, float lo, float hi);  // orig: 0x002af8b0
void spawn_particle_0b_v(const Vec4& pos, const Vec4& vel, uint32_t color, uint32_t a3, uint32_t count, uint32_t b38, uint32_t b3a, uint32_t b39, float f3c, float speed);  // orig: 0x0026a9f0
uint32_t spawn_particle_0d_v(const Vec4& pos, int32_t spin, uint32_t color, float jitter, float lo, float hi, float f30, float size);  // orig: 0x0026b790
void crate_shatter(uint32_t m, uint32_t fx, uint32_t src);                     // orig: 0x002d09b8
void crate_stack_update(uint32_t m);                                          // orig: 0x002d1428
int32_t disc_read_wait(uint32_t dst, uint32_t lba, uint32_t n);              // orig: 0x00266040
int32_t sparkle_add(const Vec4& pos, uint32_t kind, float size);              // orig: 0x001fe990
void bolt_spin_place_v(uint32_t m, Vec4* out_pos, Vec4* out_ang);           // orig: 0x002a6de8
void bolt_spin_place(uint32_t m, uint32_t out_pos, uint32_t out_ang);         // orig: 0x002a6de8
void hud_message_show(int32_t id, uint32_t value, int32_t frames);            // orig: 0x00263db0
int32_t ammo_add(uint32_t w, int32_t n);                                      // orig: 0x00233e40
int32_t pickup_collect(uint32_t m);                                           // orig: 0x002c5be8
int32_t moby_is_pickup(uint32_t m);                                           // orig: 0x0025e3f8
void wrench_pull_pickups(uint32_t m);                                         // orig: 0x002a82c0
void spawn_particle_13_v(const Vec4& pos, const Vec4& dir);                  // orig: 0x0026c6e8
uint32_t spawn_particle_15_v(const Vec4& pos, const Vec4& vel, uint32_t c0, uint32_t c1, uint32_t n, uint32_t b, float size);  // orig: 0x0026cbb0
void wrench_hit_burst(uint32_t m);                                            // orig: 0x002a8028
int32_t hit_moby_soft();                                                      // orig: 0x002a7d90
int32_t hit_moby_flag2();                                                     // orig: 0x002a7de0
int32_t moby_not_crate_or_1f6(uint32_t m);                                    // orig: 0x0025e368
void moby_rotate_by(uint32_t m, float x, float y, float z);                   // orig: 0x00262480
int32_t swept_collide(const Vec4& a0, const Vec4& a1, const Vec4& b0, const Vec4& b1, uint32_t ignore, const uint32_t* rec, int32_t n);  // orig: 0x00259d08
void throw_turn_scale_v(Vec4& v);                                             // orig: 0x002a8418
void throw_turn_scale(uint32_t v);                                            // orig: 0x002a8418
void wrench_follow_anim(uint32_t m);                                          // orig: 0x002a7c70
void cam_effect_add(uint32_t kind, uint32_t a, const Vec4* at, uint32_t b, int32_t curve, float f0, float f1, float f2);  // orig: 0x001edf60
void fx1471_slot_end(int32_t i);                                              // orig: 0x002e20f8
void moby_1471_update(uint32_t m);                                            // orig: 0x002e1df0
void moby_1007_update(uint32_t m);                                            // orig: 0x002a4e48
void wrench_update(uint32_t m);                                               // orig: 0x002a84c8
void bolt_update(uint32_t m);                                                 // orig: 0x002a5dd8
void bolt_collect(uint32_t m);                                                // orig: 0x002a6b70
void moby_floor_band(uint32_t m);                                             // orig: 0x0025a120
float floor_probe_20_v(const Vec4& pos);                                      // orig: 0x00259710
float floor_probe_20(uint32_t pos);                                           // orig: 0x00259710
void yaw_turn_damped(uint32_t m, uint32_t vel, float target, float accel, float damp, float vmax);  // orig: 0x00258278
float yaw_seek(uint32_t ang, uint32_t vel, float target, float accel, float decel, float vmax);  // orig: 0x0025be00
int32_t moby_volume_points(float density, uint32_t m, int32_t max, Vec4* out, uint32_t mask);  // orig: 0x0025aca0
void moby_hit_sparks(uint32_t m, const Vec4& vel, uint32_t mask);           // orig: 0x0025c398
uint32_t enemy_physics(uint32_t m, uint32_t p);                               // orig: 0x0025c698
void swap3_by_mask(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t mask);    // orig: 0x0025c088
uint32_t color_swap_channels(uint32_t col, uint32_t mask);                    // orig: 0x0025c0e8
uint32_t effect_spawn(uint32_t def, uint32_t pos, uint32_t arg, uint32_t dir);  // orig: 0x002d3838
void enemy_death_burst(uint32_t src, uint32_t pos, float scale, float size);  // orig: 0x0025f3e8
float angle_lerp(float a, float b, float t);                                  // orig: 0x0025b750
float round_decimals(int32_t n, float v);                                     // orig: 0x00261858
int32_t anim_crossed(uint32_t m, float t);                                    // orig: 0x002618d8
void joint_node_scale(uint32_t m, uint32_t idx, uint32_t node, float sc);     // orig: 0x00263ac8
void vec3_normalize_vu(Vec4& d, const Vec4& a);                              // orig: 0x00125180
int32_t walker_step_v(uint32_t m, Vec4& from, Vec4& to, uint32_t flags, float up, float r, float step, float slope);  // orig: 0x00258490
int32_t walker_step(uint32_t m, uint32_t from, uint32_t to, uint32_t flags, float up, float r, float step, float slope);  // orig: 0x00258490
void wander_update(uint32_t m, uint32_t r, float up, float step);             // orig: 0x00261630
void shard_update(uint32_t m);                                                // orig: 0x002acad8
void chunk_update(uint32_t m);                                                // orig: 0x002afa48
// enemy_1440.cpp
void moby_set_70(uint32_t m, float f);                                        // orig: 0x00263b38
int32_t fx1471_release(uint32_t v);                                           // orig: 0x002e2040
int32_t fx1471_set_v(uint32_t v, const Mat4& mat, const Vec4& vec);          // orig: 0x002e2190
int32_t fx1471_set(uint32_t v, uint32_t mat, uint32_t vec);                   // orig: 0x002e2190
void rand_dir_scaled_v(Vec4& d, float lo, float hi);                          // orig: 0x00257d00
void rand_dir_scaled(uint32_t d, float lo, float hi);                         // orig: 0x00257d00
uint32_t spawn_particle_45_v(const Vec4& pos, const Vec4& vel, uint32_t a, uint32_t w);  // orig: 0x00274948
uint32_t spawn_particle_45(uint32_t pos, uint32_t vel, uint32_t a, uint32_t w);           // orig: 0x00274948
void e1440_flash(uint32_t m);                                                 // orig: 0x002e1aa8
int32_t fx1471_claim(uint32_t v, uint32_t a, uint32_t b);                     // orig: 0x002e1f28
void joint_node_spring(uint32_t m, uint32_t node, uint32_t idx, float k, float d);  // orig: 0x002628d8
int32_t point_in_polygon(uint32_t p, uint32_t v, int32_t n);                 // orig: 0x00259740
void vec_yaw_marked_v(const Vec4& v, float& yaw, float& a, float& b);       // orig: 0x0025ab48
void vec_yaw_marked(uint32_t v, uint32_t yaw, uint32_t a, uint32_t b);        // orig: 0x0025ab48
void moby_anim_set_flags(uint32_t m, uint32_t set, uint32_t frame, int32_t frames, uint32_t flags);  // orig: 0x002579c8
void move_state_start(uint32_t m, uint32_t rec, uint32_t set, int32_t frames, uint32_t frame, float ang);  // orig: 0x0025c558
void color_pulse_start(uint32_t m, uint32_t rec);                             // orig: 0x0025d458
void color_pulse_update(uint32_t m, uint32_t rec);                            // orig: 0x0025d538
int32_t target_select(uint32_t self, uint32_t out, uint32_t boxes, int32_t nbox, uint32_t poly, int32_t npoly, float range);  // orig: 0x0025ff38
int32_t target_select_any(uint32_t self, uint32_t out, float range);          // orig: 0x0025fcb8
float seek_1d_v(float& pos, float& v, float target, float turn, float accel, float vmax);  // orig: 0x0025b970
float seek_1d(uint32_t pos, uint32_t vel, float target, float turn, float accel, float vmax);  // orig: 0x0025b970
uint32_t debris_spawn_v(uint32_t src, uint32_t cls, const Vec4& pos, const Vec4& ang, int32_t life, uint32_t flags, const Vec4& vel, const Vec4& spin,
                        const Vec4& size, float grav);  // orig: 0x00263fd8
uint32_t debris_spawn(uint32_t src, uint32_t cls, uint32_t pos, uint32_t ang, int32_t life, uint32_t flags, uint32_t vel, uint32_t spin, uint32_t size, float grav);  // orig: 0x00263fd8
void debris_burst(uint32_t m, int32_t cls0, int32_t n0, int32_t cls1, int32_t nrand, int32_t n1, uint32_t stay);  // orig: 0x00263e30
void explosion_burst(uint32_t src, uint32_t pos, int32_t snd, float scale, float size);  // orig: 0x0025f090
void e1440_hits_and_target(uint32_t m);                                       // orig: 0x002e1678
void e1440_update(uint32_t m);                                                // orig: 0x002e0b88
// smoke_vent.cpp
int32_t wisp_slots_alloc(int32_t n);                                          // orig: 0x002a4dc8
uint32_t spawn_particle_17_v(const Vec4& pos, int32_t spin, const Vec4& vel, uint32_t color, float jitter, float lo, float hi, float size);  // orig: 0x0026d000
uint32_t spawn_particle_17(uint32_t pos, int32_t spin, uint32_t vel, uint32_t color, float jitter, float lo, float hi, float size);  // orig: 0x0026d000
void wisp_init_rising(uint32_t m, int32_t i);                                 // orig: 0x002d7bf0
void wisp_init_hover(uint32_t m, int32_t i);                                  // orig: 0x002d7d58
void smoke_vent_update(uint32_t m);                                           // orig: 0x002d6f28
// nanotech.cpp
void nanotech_orb_pos_v(uint32_t m, Vec4& out, int32_t i);                    // orig: 0x002d8850
void nanotech_orbs_spin();                                                    // orig: 0x002d8908
int32_t nanotech_sees_hero(uint32_t m);                                       // orig: 0x002d8980
void nanotech_ground(uint32_t m);                                             // orig: 0x002d81b8
void nanotech_glints(uint32_t m);                                             // orig: 0x002d8418
int32_t nanotech_trail(uint32_t m);                                           // orig: 0x002d8510
void nanotech_update(uint32_t m);                                             // orig: 0x002d89f0
void crate_update(uint32_t m);                                                // orig: 0x002cf218
uint32_t shard_spawn(uint32_t w10, uint32_t pos, uint32_t v, uint32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t grp, float scale);  // orig: 0x002ac910
uint32_t chunk_spawn(uint32_t src, uint32_t vel, uint32_t pos, uint32_t rot, uint32_t cls, uint32_t tag, float lo, float hi);  // orig: 0x002af8b0
void rand_sphere_point_v(Vec4& out, float lo, float hi);  // orig: 0x00257d78
void rand_sphere_point(uint32_t out, float lo, float hi);  // orig: 0x00257d78
void spawn_particle_0b(uint32_t pos, uint32_t vel, uint32_t color, uint32_t a3, uint32_t count, uint32_t b38, uint32_t b3a, uint32_t b39, float f3c, float speed);  // orig: 0x0026a9f0
uint32_t spawn_particle_0d(uint32_t pos, int32_t spin, uint32_t color, float jitter, float lo, float hi, float f30, float size);  // orig: 0x0026b790
int32_t moby_sound_play(int32_t idx, uint32_t flags, uint32_t moby, uint32_t set);  // orig: 0x0028dc90
void debug_trap();                                                           // orig: 0x001ff008
extern bool g_debug_trap_is_noop;  // (oracle tests only)
void crate_settle(uint32_t m0);                                              // orig: 0x002cfcb8
void moby_class_fns_register();                                              // the ported class update functions
void render_view_setup();                                                    // orig: 0x001f6e48
void render_frame();                                                         // orig: 0x001f85b8
void render_frame_gate();                                                    // orig: 0x001f8e30
void overlay_entry_start();                                                  // orig: 0x00245c28 (its start)
void level_visit_stats();                                                    // orig: (helper) in 0x00245c28
void level_time_stat();                                                      // orig: (helper) in 0x00245c28
bool overlay_frame();                                                        // orig: 0x00245c28 (one loop pass)
// render_passes.cpp
void packet_ref_1c2f00();                                                    // orig: 0x002a0d90
void packet_ref_1c24c0();                                                    // orig: 0x002a0df0
void packet_ref_1c24f0();                                                    // orig: 0x002a0e50
void packet_ref_151900();                                                    // orig: 0x002012f0
void packet_ref(uint32_t addr, uint32_t qwc);                                // orig: 0x002a0a58
void packet_mark(uint32_t v);                                                // orig: 0x002a13a0
void render_pass_297e28();                                                   // orig: 0x00297e28
void render_static_packets();                                                // orig: 0x001f8450
void texture_uploads();                                                      // orig: 0x0024d760
void texture_block_begin(int32_t keep);                                      // orig: 0x001f9060
void texture_block_end();                                                    // orig: 0x001f9178
void draw_list_d880();                                                       // orig: 0x001f9430
void draw_list_dc80();                                                       // orig: 0x001f94a8
void draw_list_de80();                                                       // orig: 0x001f9520
void draw_list_da80();                                                       // orig: 0x001f95e8
void screen_bars();                                                          // orig: 0x001f9bd8
void dma_wait(uint32_t mask);                                                // orig: 0x002a09d8
void hud_draw();                                                             // orig: 0x0023af48
int32_t hud_frame(uint32_t key, int32_t i);                                  // orig: 0x0023b208
uint64_t hud_tex0(int32_t idx);                                              // orig: 0x0023b2b8
void hud_sprite(int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a);// orig: 0x0023b4d8
void hud_sprite_texels(int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a);  // orig: 0x0023b6c0
void hud_sprite_flip(int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a);// orig: 0x0023b860
void hud_sprite_at(uint32_t el, int32_t idx, int32_t x, int32_t y, uint32_t flags, uint64_t a);// orig: 0x0023b120
void hud_box(int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a);        // orig: 0x0023cea8
int32_t text_width(uint32_t s, int32_t n, uint32_t t);                       // orig: 0x001fb040
int32_t text_width_small(uint32_t s, int32_t n);                             // orig: 0x001fb090
int32_t text_width_big(uint32_t s, int32_t n);                               // orig: 0x001fb0d0
void glyph_sprite(int32_t x, int32_t y, int32_t w, int32_t h, int32_t u, int32_t v, int32_t tw, int32_t th, uint64_t colour, uint64_t tex0);// orig: 0x001fa290
uint64_t effect_tex0(int32_t i);                                             // orig: 0x001f9298
void text_draw(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n, uint64_t tex0, uint32_t t);// orig: 0x001fb0f0
int32_t text_centered(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n);// orig: 0x001fbad8
void text_right(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n);// orig: 0x001fb7f8
void hud_message_box(int32_t x, int32_t y, uint64_t colour, uint32_t s);     // orig: 0x0023cf80
void hud_alloc_reset();                                                      // orig: 0x002355c8
uint32_t hud_alloc(int32_t n);                                               // orig: 0x002355f0
void hud_request_flags(int32_t serial, uint32_t f);                          // orig: 0x002359f8
void hud_init();                                                             // orig: 0x00234e98
void hud_weapon_element();                                                   // orig: 0x0023adb8
int32_t hud_update(int32_t run);                                             // orig: 0x0023ac78
void hud_tex_upload(uint32_t src, int32_t dbp, int32_t psm, int32_t wl, int32_t hl, int32_t immediate);// orig: 0x0023c7b0
void hud_tex_relocate(int32_t set, uint32_t base);                           // orig: 0x002351b0
void hud_tex_upload_set(int32_t set, uint32_t base, int32_t immediate);      // orig: 0x00235490
void hud_data_decompress(int32_t set, uint32_t dst);                         // orig: 0x00240088
void hud_data_load();                                                        // orig: 0x0023fe10
void stash_init();                                                           // orig: 0x00293b18
int32_t stash_put(uint32_t src, int32_t n, int32_t qwc, uint32_t dest);      // orig: 0x00293c58
int32_t stash_write(uint32_t src, uint32_t idx, int32_t off, int32_t qwc);   // orig: 0x00293d38
int32_t stash_size(uint32_t idx);                                            // orig: 0x00293ed0
int32_t stash_read(uint32_t dst, uint32_t idx, int32_t off, int32_t qwc, uint32_t mode);// orig: 0x00293db8
void hud_element_size(uint32_t e);                                           // orig: 0x00235c80
int32_t hud_anim_frame(uint32_t a);                                          // orig: 0x0023aaa8
void hud_health_start(uint32_t e);                                           // orig: 0x002395b0
void hud_health_update(uint32_t e);                                          // orig: 0x00239630
int32_t hud_health_draw(uint32_t e);                                         // orig: 0x00239810
void hud_counter_start(uint32_t e);                                          // orig: 0x0023a760
void hud_bolts_start(uint32_t e);                                            // orig: 0x00239cc8
void hud_bolts_update(uint32_t e);                                           // orig: 0x00239d00
int32_t hud_bolts_draw(uint32_t e);                                          // orig: 0x00239df8
void hud_plain_start(uint32_t e);                                            // orig: 0x00235d80
void hud_plain_update(uint32_t e);                                           // orig: 0x00235ea0
void hud_ammo_update(uint32_t e);                                            // orig: 0x0023cdb8
int32_t hud_ammo_draw(uint32_t e);                                           // orig: 0x0023a7a8
int32_t hud_request_clear(int32_t serial);                                   // orig: 0x00235908
int32_t hud_element_place(uint32_t e, uint32_t px, uint32_t py);             // orig: 0x00235a70
void hud_element_offset(uint32_t e, uint32_t px, uint32_t py, int32_t k, int32_t d);  // orig: 0x00235ad8
int32_t hud_mode2_draw(uint32_t e);                                          // orig: 0x0023a640
int32_t text_centered_small(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n);// orig: 0x001fb9a8
void quick_select_start(uint32_t e);                                         // orig: 0x00237ae8
void quick_select_update(uint32_t e);                                        // orig: 0x00237ba0
int32_t quick_select_draw(uint32_t e);                                       // orig: 0x002382a0
void quick_select_fns_register();                                            // orig: (none)
void gadgebot_cmd_start(uint32_t e);                                         // orig: 0x00238b18
void gadgebot_cmd_update(uint32_t e);                                        // orig: 0x00238b80
int32_t gadgebot_cmd_draw(uint32_t e);                                       // orig: 0x00238f88
void gadgebot_cmd_fns_register();                                            // orig: (none)
void bomb_throw_velocity_v(const Vec4& from, const Vec4& target, Vec4& out, float g, float range);// orig: 0x002c2488
void bomb_throw_velocity(uint32_t from, uint32_t target, uint32_t out, float g, float range);// orig: 0x002c2488
uint32_t bomb_spawn(uint32_t owner, uint32_t pos);                           // orig: 0x002ace70
void bomb_throw_start(uint32_t from, uint32_t bomb, uint32_t vel);           // orig: 0x002acfd8
void bomb_glove_update(uint32_t m);                                          // orig: 0x002c26c8
void bomb_glove_fns_register();                                              // orig: (none)
void class_uv_scroll(uint32_t cls, int32_t du, int32_t dv);                  // orig: 0x0024efb0
uint32_t spawn_particle_0c(uint32_t pos, uint32_t vel, uint32_t flags, float size);  // orig: 0x0026b230
void particle_0c_update(uint32_t p);                                         // orig: 0x0026b500
uint32_t pyrocitor_nozzle_spawn(uint32_t m);                                 // orig: 0x002bb360
void pyrocitor_nozzle_update(uint32_t o);                                    // orig: 0x002bb400
void pyrocitor_flame_keep(uint32_t m, uint32_t p);                           // orig: 0x002b8798
void pyrocitor_fire_stats(uint32_t m);                                       // orig: 0x002b8230
void pyrocitor_flames_hit(uint32_t m);                                       // orig: 0x002b8438
void pyrocitor_update(uint32_t m);                                           // orig: 0x002b77f0
void pyrocitor_fns_register();                                               // orig: (none)
uint32_t spawn_particle_1b(uint32_t pos, uint32_t vel, uint32_t color, uint32_t n, float size);  // orig: 0x0026dd20
uint32_t spawn_particle_1a(uint32_t m, uint32_t color, uint32_t h, uint32_t joint, float size);  // orig: 0x0026daa0
uint32_t spawn_particle_48(uint32_t pos, uint32_t vel, int32_t mode, float k);  // orig: 0x00275158
void particle_1a_update(uint32_t p);                                         // orig: 0x0026dbe0
void particle_1b_update(uint32_t p);                                         // orig: 0x0026de00
void particle_48_update(uint32_t p);                                         // orig: 0x002752e0
float angle_seek_rate(uint32_t v, float cur, float target, float k, float damp, float vmax);  // orig: 0x00258110
void blaster_bolt_end(uint32_t o, int32_t sparks);                           // orig: 0x002cbcc0
void blaster_bolt_update(uint32_t o);                                        // orig: 0x002cb418
uint32_t blaster_bolt_spawn(uint32_t from, uint32_t pos, uint32_t target, float yaw, float pitch, float gyaw, float gpitch);  // orig: 0x002caf70
uint32_t blaster_auto_aim(uint32_t m, uint32_t pos, uint32_t yaw, uint32_t pitch);  // orig: 0x002b46a8
void blaster_update(uint32_t m);                                             // orig: 0x002b49a8
void blaster_fns_register();                                                 // orig: (none)
void particles_update();                                                     // orig: 0x00267c08
uint32_t rgba_lerp_ovl(uint32_t a, uint32_t b, float t);                     // orig: 0x0025a400
void particle_13_update(uint32_t p);                                         // orig: 0x0026c7f0
void particle_16_update(uint32_t p);                                         // orig: 0x00267b10
void particle_15_update(uint32_t p);                                         // orig: 0x0026ccb0
void particle_45_update(uint32_t p);                                         // orig: 0x00274a70
void particle_fns_register();                                                // orig: (none)
void particle_02_update(uint32_t p);                                         // orig: 0x00269290
void particle_0b_update(uint32_t p);                                         // orig: 0x0026ac78
void particle_0f_update(uint32_t p);                                         // orig: 0x0026bca8
void particle_22_update(uint32_t p);                                         // orig: 0x0026f280
void particle_23_update(uint32_t p);                                         // orig: 0x0026f660
void particle_40_update(uint32_t p);                                         // orig: 0x00273a68
void particle_1c_update(uint32_t p);                                         // orig: 0x0026e150
void particle_35_update(uint32_t p);                                         // orig: 0x002721f0
void particle_04_update(uint32_t p);                                         // orig: 0x00269a70
void particle_0d_update(uint32_t p);                                         // orig: 0x0026b910
void particle_2e_update(uint32_t p);                                         // orig: 0x002719f0
void particle_2f_update(uint32_t p);                                         // orig: 0x00271be8
void particle_29_update(uint32_t p);                                         // orig: 0x00270710
void particle_3b_update(uint32_t p);                                         // orig: 0x00273088
void particle_3e_update(uint32_t p);                                         // orig: 0x00273800
void particle_42_update(uint32_t p);                                         // orig: 0x00274308
void particle_46_update(uint32_t p);                                         // orig: 0x00274de0
void particle_12_update(uint32_t p);                                         // orig: 0x0026c558
void particle_2d_update(uint32_t p);                                         // orig: 0x00271748
void particle_3c_update(uint32_t p);                                         // orig: 0x00273298
void particle_10_update(uint32_t p);                                         // orig: 0x0026c088
void particle_17_update(uint32_t p);                                         // orig: 0x0026d1b0
void particle_19_update(uint32_t p);                                         // orig: 0x0026d700
uint32_t spawn_particle_42(uint32_t pos, int32_t colour, float size, float z);  // orig: 0x002741a0
uint32_t spawn_particle_10(uint32_t pos, uint32_t vel, uint32_t c0, uint32_t c1, uint32_t n, uint32_t h, float size);  // orig: 0x0026bed0
int32_t bomb_trace(uint32_t pv, uint32_t m, int32_t mode);                    // orig: 0x002ad410
void bomb_update(uint32_t m);                                                 // orig: 0x002adb30
void debris_7a_update(uint32_t m);                                            // orig: 0x002af5b8
int32_t light_add(uint32_t pos, float radius, float k, float r, float g, float b);  // orig: 0x0023e738
void effect_light_update(uint32_t m);                                         // orig: 0x002d3a10
void effect_light_fns_register();                                             // orig: (none)
int32_t spheres_overlap(uint32_t a, uint32_t b);                               // orig: 0x001ff4b8
void light_assign(uint32_t i);                                                // orig: 0x0023ea10
void light_reassign(uint32_t i);                                              // orig: 0x0023edc0
void level_lights_update();                                                   // orig: 0x0023e890
void dir_pack(uint32_t v, uint32_t out);                                     // orig: 0x00261480
void dir_unpack(Vec4& d, uint32_t packed);                                   // orig: 0x002615a8
uint32_t spawn_particle_02(uint32_t pos, uint32_t a, uint32_t b, uint32_t c0, uint32_t c1, uint32_t n, uint32_t h32, uint32_t h34, int32_t tex);  // orig: 0x002690b8
uint32_t spawn_particle_0f(uint32_t pos, uint32_t vel, uint32_t c0, uint32_t c1, uint32_t n, uint32_t h3a, int32_t tex, int32_t blend, float size);  // orig: 0x0026bb70
uint32_t spawn_particle_40(uint32_t pos, uint32_t vel, uint32_t n, uint32_t color, uint32_t w38, uint32_t add, float size, float a, float b);  // orig: 0x002738e8
int32_t sound_play_indexed(int32_t idx, uint32_t flags, uint32_t moby);      // orig: 0x0028dd40
void packet_scissor(int32_t x0, int32_t x1, int32_t y0, int32_t y1);         // orig: 0x002a0c68
void quad_fill(int32_t y0, int32_t y1, int32_t x0, int32_t x1, uint64_t colour);// orig: 0x001fa0e0
void box_rounded(int32_t y0, int32_t y1, int32_t x0, int32_t x1, uint32_t a);// orig: 0x001fad58
void glyph_sprite_f(int32_t u, int32_t v, int32_t tw, int32_t th, uint64_t colour, uint64_t tex0, float x, float y, float w, float h);// orig: 0x001fa648
void text_draw_f(uint64_t colour, uint32_t s, int32_t n, uint64_t tex0, uint32_t t, float x, float y, float scale);// orig: 0x001fb4f0
void text_box_set(uint32_t b, int32_t top, int32_t bottom, int32_t left, int32_t right, int32_t x, int32_t y, int32_t line, uint32_t flags);// orig: 0x001fc520
void text_box(uint32_t b, uint64_t colour, uint32_t s, int32_t n, uint64_t tex0, uint32_t t);// orig: 0x001fbf48
void text_box_draw(uint32_t b, uint64_t colour, uint32_t s, int32_t n);      // orig: 0x001fc4a8
void hud_message_text_set(uint32_t s);                                       // orig: 0x00237718
int32_t hud_message_request(int32_t who, int32_t id);                        // orig: 0x002643c8
int32_t hud_message_force(int32_t who, int32_t id);                          // orig: 0x00264470
int32_t dialog_stream_stop();                                                // orig: 0x002652e8
void help_show();                                                            // orig: 0x00203b88
void help_update();                                                          // orig: 0x00203cc0
void help_icon_draw();                                                       // orig: 0x002046c8
void help_draw();                                                            // orig: 0x002047b0
void hud_element_fns_register();                                             // orig: (none)
// game_frame.cpp (frame passes)
void help_hint_update();                                                     // orig: 0x00264328
void draw_lists_reset();                                                     // orig: 0x00297e30
void moby_free_slots_count();                                                // orig: 0x0024e7a8
void spin_timers_update();                                                   // orig: 0x001fea18
void shadow_direction_update();                                              // orig: 0x0024fba8
void frame_counters_advance();                                               // orig: 0x00297e70
// sound.cpp
void snd_play_reply(int32_t reply, uint64_t data);                           // orig: 0x0028e108
void snd_set_reply(int32_t reply, uint64_t data);                            // orig: 0x0028e150
void sound_listener_point_v(Vec4& p);                                        // orig: 0x0028c760
void sound_listener_point(uint32_t p);                                       // orig: 0x0028c760
uint32_t sound_line_blocked_v(uint32_t slot, const Vec4& from);                // orig: 0x0028c810
int32_t sound_pan(uint32_t slot, uint32_t pos, const Mat4& m);               // orig: 0x0028c9e8
int32_t sound_frame();                                                       // orig: 0x0028cc08
void sound_stop_all();                                                       // orig: 0x0028e048
int32_t level_sound_play(int32_t idx, uint32_t flags, uint32_t emitter, int32_t volume);  // orig: 0x0028ddd8
void emitter_sphere_update(uint32_t em);                                     // orig: 0x002ed950
void emitter_box_update(uint32_t em);                                        // orig: 0x002edad8
void sound_emitters_update();                                                // orig: 0x0028dfc8
void stream_system_init();                                                   // orig: 0x00264b68
void sound_system_init();                                                    // orig: 0x0028ca88
void sound_callbacks_register();                                             // the EE callbacks of 989snd's replies

}  // namespace rac1

#endif
