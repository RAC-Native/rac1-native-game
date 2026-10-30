// The "Advanced" options: settings of this PC version that the original does not have. Each one is off by default, so the
// game plays as the original unless
// the player turns it on. They are shown as a page of the pause menu's Options (Options >
// Advanced), built from the original's own widgets: the page, its title and its settings widget
// are copies of the Camera page's, kept in a block of EE RAM the game never uses. The settings
// are kept with the saves (save<N>.pc next to the game's save<N>.bin, which stays untouched) and
// as the player's preferences (~/.config/rac1-native/advanced_options.txt).
// (Fades, is a console compromise removed rather than an enhancement: its default, 0, is
// the port's fast fade; 1 the original's.)
#ifndef RAC1_GAME_ENHANCE_ADVANCED_H
#define RAC1_GAME_ENHANCE_ADVANCED_H

#include <cstdint>
#include <string>

namespace rac1 {

// Every enhancement off and the preferences file never read (the oracle: the faithful port alone)
extern bool g_advanced_disabled;

// orig: (none) — enhancement: called by menu_open (0x276190): the Advanced page put in the
// Options page of the current overlay (the entry before Quit Game, its help text), the settings
// loaded from the file the first time
void advanced_menu_install();

// orig: (none) — enhancement: the text of an id of the Advanced page (0x7f00..), 0 for the
// game's own ids; looked at first by the text lookup 0x203ad8
uint32_t advanced_text(int32_t id);

// orig: (none) — enhancement: a menu picture of ours (its table entry's sector PICTURE_SECTOR +
// n, made by the host): written into the buffer `buf` (a 256 x 256 8-bit image) and true, false
// for the game's own pictures (read from the disc)
bool advanced_picture(uint32_t sector, uint32_t buf);
// orig: (none) — enhancement: true (once) when `buf` holds a picture of ours, not to be unpacked
bool advanced_picture_take(uint32_t buf);

// orig: (none) — enhancement: the game wrote save<slot>.bin (its save of the game, the autosave
// of a planet change, a new game's default save): the settings written next to it (save<slot>.pc)
void advanced_save_written(int32_t slot);
// orig: (none) — enhancement: the game loaded save<slot>.bin: the settings of its save<slot>.pc
// applied (none: an older save, the settings kept as they are)
void advanced_save_loaded(int32_t slot);

// orig: (none) — enhancement: the settings widget (0x27ef00) changed the byte `byte`: the settings
// that go together kept so (Aiming = Modern needs Movement = RAC 3)
void advanced_setting_changed(uint32_t byte);

// orig: (none) — enhancement: the settings widget (0x27ef00) `w`, its selected row `e`: left / right
// on our pages' rows (Advanced, Graphics) step the value back / forward; true when it did
bool advanced_settings_dpad(uint32_t w, uint32_t e);
// orig: (none) — enhancement: the update check's preferences (Options > Advanced: Update Check, Update
// Channel; "Skip Version" of the update notice, update_notice.h): read from the preferences file
// when the game's memory does not hold the settings yet (at start). Check off while every
// enhancement is off (headless runs).
struct AdvancedUpdatePrefs {
    bool check = true;         // Update Check = On
    bool development = false;  // Update Channel = Development (main), else Stable (the releases)
    std::string skipped;       // the release's tag / main's commit "Skip Version" chose
};
AdvancedUpdatePrefs advanced_update_prefs();
// orig: (none) — enhancement: "Skip Version": `id` kept in the preferences
void advanced_update_skip(const std::string& id);

// orig: (none) — enhancement: the Advanced page (built by advanced_menu_install) and its title's text
// id (the main menu's Options list)
uint32_t advanced_page();
int32_t advanced_title_text();

// orig: (none) — enhancement: once a gameplay frame (0x297f78): the settings saved when changed
void advanced_frame();

// orig: (none) — enhancement: the world holds still this frame (no hero, mobys, particles,
// camera): "pause in the quick select" is on and the quick select is open, triangle held
bool advanced_world_paused();

// orig: (none) — enhancement: Options > Advanced > Buy All Ammo is On: the Vendor's ammo list has the "All Ammo" entry
// first (vendor_buy_all.h)
bool advanced_vendor_buy_all();

// --- Fast fades ---
// orig: (none) — a console compromise removed (faithful to the game's code, not to the PS2's limits): the screen fade
// 0x1f9838 (it hid the PS2's disc loading) runs its loop whole - the same RAM writes, the frame
// counter 0x15f3f8 + n + 2 - inside one host frame, nothing of it shown. True: "Original fades"
// (Options > Advanced > Fades, the debug menu, RAC1_FADES=original): its frames shown, one a host frame.
bool advanced_original_fades();
// orig: (none) — the setting changed (the debug menu, the oracle)
void advanced_set_original_fades(bool on);

// --- The modern controls ---
// orig: (none) — enhancement: pad_build's held buttons of pad `pad` remapped (Modern, in gameplay:
// L2 taken as the aim, R2 as circle)
uint32_t advanced_pad_remap(uint32_t pad, uint32_t held);
// orig: (none) — enhancement: strafing now (Movement = RAC 3: L2 / R2 held — L2 with Aiming =
// Modern —, gameplay, the hero on the ground or jumping): facing held, moving along the stick
bool advanced_strafing();
// orig: (none) — enhancement: aiming now (strafing with Aiming = Modern, which needs RAC 3): the
// shoulder view, the pitch, the reticle, the weapons along the camera
bool advanced_aiming();
// orig: (none) — enhancement: the hero's turn spring (0x211e30) asks for its target heading: while
// aiming the camera's heading (and the spring's constants), else `target`
float advanced_turn_target(float target, float& k, float& d, float& vmax);
// orig: (none) — enhancement: the stick's move speed (+0x190) reduced while aiming
void advanced_move_speed();
// orig: (none) — enhancement: around the hero's update: standing still while aiming, the hero turned
// to the camera when no state did
void advanced_aim_before_hero();
void advanced_aim_after_hero();
// orig: (none) — enhancement: around the camera's update: while aiming the right stick turns the aim
// (not the camera), and the follow camera is held on the aim's heading afterwards
void advanced_aim_before_camera();
void advanced_aim_after_camera();
// orig: (none) — enhancement: the weapons' "aim along the camera" test: the original's L1 / L2 held
// (`pad_held & 5`, the first-person view), or the modern controls aiming
bool advanced_camera_aim(uint32_t pad_held);
// orig: (none) — enhancement: while aiming, a shot's direction (`dir`, length `speed`) from the
// point `from` toward the point under the screen's centre (the first thing the camera's line meets
// within 80 units, else that far)
void advanced_aim_direction(uint32_t from, uint32_t dir, float speed);
// orig: (none) — enhancement: the other weapons' shots: while aiming, the direction `dir` (computed
// by the original from the hero's facing) turned toward the point under the screen's centre, from
// the point `from` (or the hand moby's chain point 0), its length kept; nothing otherwise
void advanced_aim_shot(uint32_t from, uint32_t dir);
void advanced_aim_shot_hand(uint32_t hand, uint32_t dir);
// orig: (none) — enhancement: while aiming (not leaning on a wall), the weapon's frame `rows` (three rows:
// forward, side, up; the original copies the hero's) turned onto the aim from the hand's chain point 0
void advanced_aim_frame_hand(uint32_t hand, uint32_t rows);
// orig: (none) — enhancement: while aiming, the yaw / pitch of that shot from `from`, in the weapons'
// convention (atan2_f(x, y), -atan2_f(horizontal length, z)); false (and nothing set) otherwise
bool advanced_aim_angles(uint32_t from, float& yaw, float& pitch);

// --- The mouse and the PC's weapon actions (#117, #61) ---
// What the host gives the game once a game frame, beside the pad's packet (src/host/host_input.h): the Direct mouse's
// angles (radians, right / down positive; 0 unless the host asked advanced_mouse_direct and it said yes) and the PC
// actions' buttons held
struct AdvancedHostInput {
    float look_x = 0.0f, look_y = 0.0f;  // at the look sensitivity: the follow camera, the strafe's heading
    float aim_x = 0.0f, aim_y = 0.0f;    // at the aim sensitivity: the modern aim
    bool next_item = false, prev_item = false;
};
// orig: (none) — enhancement: the host's input of the game frame about to run (pad_packet)
void advanced_host_input(const AdvancedHostInput& in);
// orig: (none) — enhancement: whether the game takes the Direct mouse now: the follow camera ran the last game frame,
// in gameplay (else the host gives the mouse as the stick: the first-person view, the menus, other cameras)
bool advanced_mouse_direct();
// orig: (none) — enhancement: the follow camera's place (cam0_place_frame 0x2e9bb0), after its stick input (`b` the
// pitch input, cam0_stick 0x2e89b0): the Direct mouse turns the offset (yaw) and holds the pitch spring where the
// mouse put it; nothing without mouse movement
void advanced_mouse_camera(uint32_t cam, float& b);

// --- R&C 3's on-foot movement ---
// orig: (none) — enhancement: Movement = RAC 3
bool advanced_rac3_movement();
// orig: (none) — enhancement: run_update's "move along the stick" flag (+0x3b8), R&C 3's rule
void advanced_rac3_along_stick();
// orig: (none) — enhancement: run_update's turn (R&C 3's springs, the low-stick target cut); the
// acceleration factor (0: none while turning sharply)
float advanced_rac3_run_turn();
// orig: (none) — enhancement: run_input's stop: the speeds capped before standing at once
void advanced_rac3_stop();
// orig: (none) — enhancement: enter_jump's jump 7 / 9 parameters, R&C 3's
void advanced_rac3_jump();
// orig: (none) — enhancement: run_jump's choice: the running jump 9 (else 7), R&C 3's rule
bool advanced_rac3_running_jump();
// orig: (none) — enhancement: swing_input (states 0x13 / 0x15) leaving to the run with the stick,
// R&C 3's rule; true when it did (`step`: the combo table's row)
bool advanced_rac3_swing_exit(uint32_t step);
// orig: (none) — enhancement: swing_input's look-back for the jump press (`back`, from the combo
// row's +0xc); R&C 3's throw (state 0x15) looks back 8 frames at 60 Hz (frame-scaled)
int32_t advanced_rac3_swing_jump_back(int32_t back);
// orig: (none) — enhancement: R&C 3's guard while the thrown wrench is out (thrown flag +0x10aa
// set): no item input, no crouch throw, no new throw start
bool advanced_rac3_wrench_out();
// orig: (none) — enhancement: run_input's jump while aiming: the flip 0xb instead (stick sideways /
// back past 0.9), as R&C 3's strafe
bool advanced_strafe_flip();
// orig: (none) — enhancement: the strafe flip just entered: its velocity along the stick only
void advanced_strafe_flip_entered();
// orig: (none) — enhancement: hero_horizontal_velocity (states 0xb / 0xc): a strafe flip under way,
// and its target speed (R&C 3's)
bool advanced_strafe_flip_active();
float advanced_strafe_flip_speed();
// orig: (none) — enhancement: after a moby's joint matrices are posed (moby_skin_all 0x2558e0, the
// record `rec`): Ratchet's lower body turned towards the strafe's movement
void advanced_pose_hook(uint32_t rec);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_ADVANCED_H
