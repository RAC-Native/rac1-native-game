// See variants.h / variants_l14.h. Every row was checked by diffing the two functions' instruction streams (relocations and
// registers masked, the jal targets compared through the levels' maps, then the immediates, the cross-level
// compare of): same source, other build.
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>

namespace rac1 {

namespace {
constexpr VariantRow kOltanis[] = {
    // Veldin function, level 14's address, bytes
    // the same code as Veldin's (the matcher could not place them: Veldin's size counts the alignment padding / the next function's first words after the return)
    {0x00259028, 0x002615a0, 0x188},  // doomlet_259028
    {0x00262e40, 0x0026ad28, 0x278},  // path_point
    {0x00263618, 0x0026b1b8, 0x1d8},  // glow_draw
    {0x002655d8, 0x0026cef0, 0xe0},   // music2_start
    {0x0028dd40, 0x00293f98, 0x90},   // sound_play_indexed
    {0x0028dfc8, 0x002942d0, 0x78},   // sound_emitters_update
    {0x0028e990, 0x00294d08, 0x30},   // ship_hide
    {0x002a4530, 0x002aa8d8, 0x320},  // region_height
    {0x002ec678, 0x00313178, 0x30},   // cam7_step
    // level 14's own code (transliterated at its addresses: oltanis_hero_tr_*.cpp)
    {0x00205000, 0x00218610, 0xa0},   // sub_00205000
    {0x002062b0, 0x002197e8, 0x3a8},  // hero_move_mode (the Novalis family's 0x3a8-byte build; Veldin's is 0x958 bytes)
    {0x002070d0, 0x0021a058, 0x1f0},  // hero_frame_update
    {0x002076a8, 0x0021a628, 0x170},  // sub_002076a8
    {0x0020b960, 0x0021dd30, 0x298},  // hero_surface_update
    {0x0020bed0, 0x0021dfc8, 0x7a8},  // hero_ledge_grab_check
    {0x0020c758, 0x0021e770, 0x568},  // hero_ledge_check_v
    {0x00210a08, 0x00222870, 0x108},  // hero_mode_enter
    {0x00210b30, 0x00222978, 0xc8},   // hero_mode_exit
    {0x00210ce8, 0x00222aa8, 0x360},  // hero_hurt
    {0x00211380, 0x00222f38, 0x260},  // hero_gait_update
    {0x00211be8, 0x00223710, 0xb8},   // hero_stick_speed
    {0x00212318, 0x00223de8, 0x348},  // hero_wall_slide
    {0x002127b8, 0x00224230, 0x7c8},  // hero_ground_update
    {0x002133a8, 0x00224db0, 0x3c0},  // hero_collide_push
    {0x00214658, 0x00226020, 0x7f8},  // hero_horizontal_velocity
    {0x00217970, 0x00228cd0, 0x6230}, // hero_state_update
    {0x002215c8, 0x0022f1b8, 0x510},  // hero_timers_update
    {0x002223f8, 0x0022ff58, 0x3780}, // hero_set_state
    {0x00227638, 0x002336d8, 0x58},   // hero_enter_idle
    {0x00227fa0, 0x00233a48, 0xbc0},  // hero_item_use
    {0x00229b70, 0x00235600, 0x5a48}, // hero_input_update
    {0x00233210, 0x0023c550, 0x18},   // hero_mobys_set_flag1
    {0x00233248, 0x0023c568, 0x20},   // hero_mobys_clear_flag1
    // level 14's cutscene_level_extras (its scene 0 branch only: Veldin's port runs it, cutscene.cpp)
    {0x0023e268, 0x002460f8, 0x78},   // cutscene_level_extras
};
}  // namespace

// orig: (host) see variants.h
const VariantRow* ovl_variants_l14(size_t* count) {
    *count = sizeof kOltanis / sizeof kOltanis[0];
    return kOltanis;
}

}  // namespace rac1
