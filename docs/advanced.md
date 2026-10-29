# Advanced options

Options > Advanced holds the settings that change how the game plays. They are different in kind
from the PS2 compromises the port removes (see [Overview](overview.md#faithful-to-the-games-code-not-to-the-ps2s-limits)):

- **A removed compromise** keeps the game's behaviour and state exactly the original's and only drops
  something the console needed, such as fades that hid disc loading. It is on by default, with the
  original's way kept as an option.
- **An enhancement** changes the game's behaviour or state: how Ratchet moves, how the controls map,
  whether the world pauses. Enhancements are **optional, off by default, and labelled as changes to
  the game**. With all of them off, the game plays as the original.

The Advanced page has these rows:

| Row | Values (default first) | Kind |
|---|---|---|
| Quick Select | Normal, Pause | enhancement |
| Aiming | Original, Modern, Modern, No Zoom | enhancement |
| Movement | Original, Strafing, RAC 3 | enhancement |
| Fades | Fast, Original | a removed compromise: Fast is the port's default, Original the PS2's |
| Buy All Ammo | Off, On | enhancement |
| Vibration | Off, Low, High | enhancement (kept as the machine's preference, not with the saves) |
| Ship | Original, Ship 1, Ship 2, Ship 3 (only the ships the save has) | enhancement (its look only) |
| Clank | Original, Modern | enhancement |

All of it is in `src/game/enhance/advanced.{h,cpp}` (Buy All Ammo's Vendor entry in
`src/game/enhance/vendor_buy_all.{h,cpp}`). None of it is a port: every function there is annotated
`// orig: (none) — enhancement`.

## How the page is built

The page is made from the game's own menu data, so it looks and behaves like the game's pages. When
the pause menu opens (`menu_open`, 0x276190, calls `advanced_menu_install`), the port copies the
original's Camera page (0x1b5a60), its title and its "settings" widget (the widget at 0x27ef00 whose
rows are a name, the byte it sets and up to four value texts; Cross cycles the value) into a block
of EE memory the game never uses (0xf0000 to 0xf2000, its texts at 0xfb000 to 0xfe000, inside the PS2 kernel's first megabyte, which
the port does not use since its kernel is the host). It then writes its own rows and texts there and
inserts Graphics, Controls and Advanced into the Options list before Quit Game. The main menu gets
the same pages (`title_menu_page_start`). The game's text lookup (0x203ad8) asks `advanced_text`
first for the page's own text ids (0x7f00 and up), in English, German, Spanish and Italian (French
shows English for now).

Each setting is one byte in that block, and value 0 is the original's behaviour (for Fades, the
port's fast fades). Left and right on a row step the value (`advanced_settings_dpad`).

**Where the settings are kept:**

- as the player's preferences, in `~/.config/rac1-native/advanced_options.txt` (or
  `$XDG_CONFIG_HOME/rac1-native/`, or `$RAC1_ADVANCED_FILE`), one `name value` line per setting,
  written whenever a setting changes;
- with each save, in a `save<N>.pc` file next to the game's own `save<N>.bin` (see
  [Saves](saves.md#the-pc-settings-next-to-each-save)), so each playthrough keeps its own. The update
  check's two rows are not: they belong to the machine (in the file, `update_check_off` and
  `update_channel`, and `update_skip`, the version the update notice's Skip Version chose).

**The New Game choice.** Picking "Modern mode" or "Experimental" at New Game turns Quick Select, Aiming
(Modern), Movement (RAC 3), Buy All Ammo and Vibration (High) on; Experimental also turns Clank (Modern)
on; "Original mode" turns them all off (`advanced_apply_mode`). Ship goes back to Original in every mode
(a new game has the first ship only). Fades is left as it is. Every setting can be changed at any time
afterwards. The Controls page's mouse settings are the machine's and no mode changes them.

**Linked settings.** Modern aiming is built on the strafe, so turning Aiming to Modern when Movement
is Original turns Movement to Strafing (keeping RAC 3 if already chosen), and turning Movement back
to Original turns Aiming back to Original (`advanced_setting_changed`).

## Quick Select: Pause

In the original, the world keeps moving while the quick-select ring is open (Triangle held). With
Pause, it holds still.

`advanced_world_paused` is true when the setting is on, Triangle is held and the HUD's quick-select
element is the one open (its update function is 0x237ba0, or 0x238b80 for the variant the hero opens
in his other mode). `gameplay_frame` (0x297f78, `src/game/ovl/game_frame.cpp`) then skips the hero,
the mobys, the particles and the camera for that frame, and the play-time statistic does not count it.

## Vibration (#10)

The original has no vibration (it came with *Going Commando*). With **Vibration = Low** or **High**, the
gamepad's motors answer what happens, in the spirit of *Ratchet & Clank 2* and *3*: damage taken (stronger
the more nanotech is lost; a hit on the ship's shield too), Ratchet's death, a hard landing, explosions
nearby, each weapon's shot (light ticks for the Blaster, the Pyrocitor and the Tesla Claw, medium for the
gloves and the others, heavy for the Devastator and the R.Y.N.O.), the wrench's hit, a subtle tick for the
bolts picked up, and a low hum while the ship flies. Low is a little under half of High. It is off by
default and in Original mode; Modern and Experimental set it to High.

**The game is only read.** Once a gameplay frame `vibration_frame` (`src/game/enhance/vibration.cpp`,
called by `advanced_frame`) compares what the game's own code did with the frame before: the nanotech
(+0x22a8) and the hits counted by `hero_hurt` (0x15eea8), the hero's state (the death states 0x3d and
0x5d) and the lost-hero flag (+0x20b1), the fall speed when the jump states end, a new camera shake
(0x166de0 / 0x166de8, which the weapons' blasts, mines and bombs set when they go off in view, bigger the
closer), the ammo of the item in hand (0x13d428 + item x 4), the wrench swing's hit flag (+0xa64, set by
`wrench_hit_sparks` 0x217368), the bolt count (0x15ed98) and the ship mode (+0x20a4 = 2). Reading the
results instead of hooking the functions covers every level's own build of the hero and weapon code
(several levels run transliterated copies) with one reader, and cannot change the game: a run with
Vibration on has the same RAM hashes as one without, outside the menus' own block. Each event starts a
pulse of the two motors that fades over a few frames; the host takes the motors once a game frame
(`vibration_take`: 0 when no gameplay frame ran, so menus and pauses still them).

**The motors** (`src/host/rumble.cpp`): GLFW has no force feedback, and no library is added for it:
- Linux: the kernel's force feedback (`FF_RUMBLE`) on the pad's event device, found among
  `/dev/input/event*` by the same bus / vendor / product / version numbers GLFW's GUID is made of. The
  kernel's drivers cover the Xbox pads (`xpad`), the DualShock 4 and the DualSense (`hid-playstation`,
  `hid-sony`), the Switch Pro (`hid-nintendo`) and most others. The desktop gives the seated user access
  to a gamepad's event device; without it the log says so and nothing vibrates.
- Windows: XInput (`XInputSetState`, from `xinput1_4.dll` or `xinput9_1_0.dll`, loaded when first
  needed) on every connected Xbox-compatible pad. A DualShock 4 or DualSense on Windows is a HID device
  that XInput does not see: it vibrates through Steam Input or DS4Windows (which present it as an Xbox
  pad), not by itself.
- macOS: none.
The pad driven is the one last used (the one the Controls page names). A setting lasts one second and is
renewed every half second while the motors run, so a stalled game never leaves the pad buzzing.
`RAC1_VIBRATION_LOG` prints the motors as they change (with or without a pad).

## Ship (#94)

Ratchet's ship changes twice in the story. With **Ship** set to one of the ships the save has, that ship
is the one shown, on every planet, from the next level load (the ship is read with the level); Original
shows the game's own choice. The row offers only the ships unlocked so far (left, right and Cross skip
the others); a save loaded with a ship it does not have yet shows the game's.

How the game picks it: at each level exit (`level_exit_start`, 0x292e08) it sets 0x13e056 to 0, then
1 after the story point 0x13dd48 or from level 8 on, then 2 after 0x13dd4e or from level 14 on. That
index names the "armor" file read with the level (the disc's table of contents 0x138e48[index + 1]),
which holds the ship's two classes (0x160548[index] = 0x213 to 0x215, the ship, and 0x160558[index] =
0x217 to 0x219) with their models and textures (`armor_load`, 0x23fa00), and the ship variant whose
tables place its flames, its trails and Ratchet's and Clank's seats in the ship sequences.

What the option changes: the chosen ship's armor file is read instead (`advanced_ship_armor`, in the
level loader's three places: `level_files_read`, `level_load_step`, `armor_load`'s size), and its
models are registered as the game's own ship classes; the sequences' flames, trails and seats and the
third ship's tint follow the ship shown (`advanced_ship_look`, `advanced_ship_look_class`). Everything
else keeps the game's index: 0x13e056 itself, the ship moby's class, the landing and take-off scenes
and their sounds, the story's special cases (Orxon, where Clank goes alone; Oltanis; Gemlik's ship),
the ship's hatch and the saves. The ship's collision is its model's, so it is the shown ship's shape.
Checked on Kerwan with the third ship unlocked: the landing scene, Ratchet's place and the scene's
timing are the game's, with the first ship's model in it. Gemlik's space fight with Qwark flies the
armor's ship (the game spawns 0x160548[index] there), so it shows the chosen ship too; its stats are the
game's. The flight between planets on the loading screen shows the ship of the level being left. Not
checked in a run: the space fight and every planet's take-off.

## Clank: Modern (#57)

Clank's sections (Orxon, where he goes alone) are slow next to Ratchet's RAC 3 movement. With **Clank =
Modern**, in the spirit of Clank's puzzles in *A Crack in Time* and *Rift Apart*:

- **His walk** (state 0x44, `wade_update` in `hero_state_update.cpp`, which Ratchet's wading 0x50 shares:
  only Clank's is changed) is 5 units/s at full tilt instead of 3, gets there faster (acceleration 25 /
  braking 30 instead of 7.5 / 8.5) and turns with the tighter turn of his slow walk at 1.5 times its rate.
- **His jump** (state 0x45) steers at 5 units/s eased at 20 (3 and 12 in the original): more control in
  the air. The jump's height is the original's.

His movement was already relative to the camera (the stick through `hero_stick_move` mode 0, as
Ratchet's). The option is tested by `advanced_clank_modern`: the setting on and the hero in Clank's mode
(+0x20a4 = 1); Giant Clank (Quartu) is untouched. Measured on Orxon's start: over the same 200 frames of
stick forward Clank covers 14.7 units instead of 9.0.

A first step. Not done yet: Quartu's small-Clank section (Quartu runs its own build of the hero code,
which these hooks do not reach), a higher or longer jump, a run (a faster gait past a tilt, with its
animation), and the Gadgebots' commands.

## Buy All Ammo

*Ratchet & Clank: Up Your Arsenal*'s Vendor can fill every weapon's ammo in one purchase. With **Buy All
Ammo = On**, R&C1's Vendor has the same: an **All Ammo** entry before the first item of its list, on the
Vendor's screens and on the PDA's.

- **An entry of the list.** The entry is a tile of the Vendor's strip like the others, the first one:
  the strip, its highlight and its scrolling are the game's own, so it moves, scrolls and wraps exactly
  as the weapons do. Its tile shows four of the list's weapons, a quarter each. Selected, the item
  monitor shows "All Ammo" and the price of filling every weapon of the list that is not full, and the
  sign over the Vendor says "ALL AMMO".
- **Opened on it.** With the option on, the Vendor's screens (and the PDA's) open with the entry
  selected; the game opens them in the middle of the list.
- **Moving and wrapping are the game's.** The game's Vendor has one list: the weapons for sale and the
  ammo of the weapons Ratchet owns, in one strip (there is no separate weapons screen). With fewer than 8
  items the strip is fixed and does not wrap: Left stops at the first item, Right at the last. With 8 or
  more it is a turning wheel that wraps both ways. The entry is the list's first item, so it counts
  towards the 8 (a list of 7 weapons becomes a wheel) and, on a wheel, Left from the entry goes to the
  last weapon and Right from the last weapon comes back to the entry.
- **The price** is the game's own: for each weapon, its missing rounds times its price a round (the item
  table's), written as the game writes prices. On the PDA, the monitor shows the PDA's dearer total and,
  struck through, the Vendor's, as the game does for one weapon.
- **Cross buys**, weapon by weapon in the list's order, with the steps of the game's own ammo purchase
  (0x29ba78): the rounds added by the game's ammo function (0x233e40), the ammo bought counted
  (0x13dd70), the bolts taken. Then, once, what the game does after a purchase: the Vendor's thanks,
  the list rebuilt, the sign's "thank you" and the purchase sound.
- **Too few bolts:** each weapon in the list's order gets as many rounds as the bolts left pay for (a
  weapon whose round costs more than what is left is passed over, and the next ones still get theirs),
  so the bolts end as near 0 as the prices allow. With not enough bolts for a single round of any
  weapon, the game's own "not enough bolts" message and error sound; with every weapon full, the
  game's "full" sound and a price of 0. (UYA's own rule here was not checked; this is the likely
  expectation.)
- **Off, the game's Vendor is untouched:** nothing of the entry runs, and the game's state is the
  original's (checked with RAC1_RAMHASH on Vendor runs with moves, a purchase and the PDA).

How it is built: the entry is put into the game's own item list (0x1ca4c0) by `item_list_build`
(0x29a400), first, as an ammo entry of one of the list's weapons (the one not full with the cheapest
round, else the first), marked by its third word, which the game leaves 0 and tests before it starts a
purchase: Cross on the entry starts none, `vendor_buy_all_input` (before the Vendor's update in
`overlay_entry.cpp`) buys instead. Everything else (the cursor, the strip, the wheel, the highlight,
the sounds) is the game's code walking its list. The port steps in only at a few calls of the Vendor's
transliteration (`tools/codegen/gen/mkall/veldin_vendor.glue`, each told its call site): the tile's
sprites (four weapon tiles at a quarter size, with the game's `hud_frame` / `hud_sprite`), the monitor's
two title lines and its price, the sign's text when the cursor arrives on the entry, and the cursor put
on the entry when the screens open. The hologram on the stand, the monitor's picture and the ammo counter
at the top left are the game's own for the entry's weapon. The list is not saved (the save's block lists,
0x1845c0 and 0x1848c0, do not include it). The list holds 16 entries at most: a list that is already
full gets no entry (no Vendor list reaches it in normal play). Every planet runs the same Vendor code,
relocated (the overlay maps have no variant of it). `RAC1_VENDOR_TRACE=<n>` prints the Vendor's state,
the bolts and each entry of the list every n frames (testing).

## Fades: Fast or Original

The game's screen fade (0x1f9838) waited for n + 2 vsyncs, hiding the PS2's disc loading. By default
the port runs the fade's loop whole, with the same memory writes and the frame counter moving by
n + 2, but inside one host frame: the screen cuts. With Original, each of its frames is shown, as on
the PS2.

The fade's vsync wait is `fade_vsync` in `src/game/ovl/screen_fade.cpp`, which calls
`ee_thread_vsync(kind, advanced_original_fades())`: when the second argument is false, the game's
coroutine does not suspend there (see
[Game logic](game-logic.md#the-game-frame-on-a-coroutine)). `RAC1_FADES=original` or `fast` sets
it from the environment.

## Movement: Strafing and RAC 3

Ratchet & Clank 3 (Up Your Arsenal, UYA) moves Ratchet differently from the first game: the run
follows the stick's tilt, he turns and stops quicker, his jumps leave the ground sooner, and L2 or R2
held makes him strafe.

- **Movement = Strafing** adds the strafe (L2 or R2 held, or L2 with Aiming = Modern) while keeping
  R&C1's original movement parameters: run speeds, acceleration, turning, jump heights, long jumps,
  side flips and wrench timings stay the original's.
- **Movement = RAC 3** brings UYA's full on-foot movement and wrench feel to the first game's hero
  code, together with the strafe.

### How it was built: a study of UYA's code

The values and rules were taken from a study of UYA's own hero code, on the project owner's own
NTSC-U disc of Up Your Arsenal (`SCUS-97353`):

- UYA's level overlays were extracted from the disc and its boot executable unpacked; one ELF was
  built from the core sections and level 1's overlay and loaded into Ghidra with the Emotion Engine
  plugin.
- UYA's hero functions were found by their constants and callers. The stick-to-movement function was
  recognised by the same axis-snap constants as R&C1's, and the rest followed from its callers. Each
  was matched to its R&C1 counterpart:

  | Role | UYA (level 1 overlay) | R&C1 (Veldin) |
  |---|---|---|
  | the hero's per-frame tick | 0x4ea708 | 0x211670 |
  | the state update | 0x4f2090 | 0x217970 |
  | the input transitions | 0x503b68 | 0x229b70 |
  | entering a state | 0x4fc518 | 0x2223f8 |
  | the stick's tilt | 0x4ea9a8 | 0x211830 |

  plus UYA's stick speed (0x4eadd0), turn spring (0x4eaff8), horizontal and vertical velocity
  (0x4ede80, 0x4ed860), gait (0x5024b8), the strafe's functions (0x4ea5b0, 0x4ea628), and its wrench
  combo table (0x2f6310).
- The turn springs were modelled numerically to compare turn times, and R&C1's own speeds were checked
  against RAM snapshots of the game running in PCSX2.

**Nothing from UYA is in this repository.** No code or data of the other game was copied. What was
taken are **numbers and rules**, quoted in `advanced.cpp`'s comments with the UYA address they were
read at. UYA's code is not ported: R&C1's ported functions are given UYA's values or rule at a few
specific points, and with Movement = Original those points run the original's code untouched.

### What changes, R&C1 against UYA

| | R&C1 (Original) | UYA (RAC 3) |
|---|---|---|
| Stick tilt | the stick's length, at most 1 | the mean of its square and round lengths, at most 1: full tilt sooner on the diagonals |
| Run speed | two steps: 0.9 or 5.7 units/s | proportional to the tilt, 6.17 units/s at full tilt |
| Acceleration / deceleration factors (/3600) | 7.5 / 8.5 (46 frames to top speed) | 37 / 30 (about 10 frames) |
| Run turn spring (stiffness, damping, max speed) | 0.008, 0.15, 0.1658 | 0.037, 0.13, 0.2036 (walking: stiffness and max speed scale with the stick) |
| Moving along the stick instead of the facing | below 0.3 units/s with the stick past 0.85, until within 3° | below 1.7 units/s, until within 5° |
| Letting go of the stick while running | a skid state, or 25 more frames of running | straight to standing, speed and drift capped at 1.5 units/s |
| Jump take-off | 5 frames after the press | 1 frame |
| Jump height (base, rising to, over frames held) | 1.47, 2.62, 15 | 1.42, 2.70, 17 |
| Air speed | 5.7 | 6.17 |
| Double jump air speed | 3.5 | 4.75 |
| Air braking with the stick released (/3600) | 2 | 8 |
| Running jump (the long one) | when running, facing within 60° of the stick and fast enough | when the tilt is past 0.85 and not strafing |

**The wrench.** The animations, lunges and hit frames of the combo are identical in both games;
only some timing fields of the combo table differ (rows 0 to 3: the next swing's window, the row's
end, the throw's jump-cancel). With RAC 3 the current overlay's combo table (0x17bc28) holds UYA's
values; the original values are put back when the setting is turned off, and a level load brings its
own table (`combo_table_sync`). UYA also lets Ratchet run straight out of a swing or a throw late in
its animation when the stick is pushed (`advanced_rac3_swing_exit`).

### Where it plugs into R&C1's hero code

Every hook is a call from a ported function into `advanced.cpp`, at the point where UYA's rule differs:

| Ported function (orig address) | File | Hook | What it changes |
|---|---|---|---|
| `pad_build` (0x2671b0) | `src/game/input/pad_build.cpp` | `advanced_pad_remap` | takes L2 and R2 for the strafe (and R2 as Circle with modern aiming) before the game sees the pad |
| `hero_stick_speed` (0x211be8) | `src/game/ovl/ovl_21.cpp` | `advanced_move_speed` | the run speed proportional to the tilt |
| the velocity heading (0x2120d8) | `ovl_21.cpp` | `advanced_strafing` | while strafing, the velocity follows the stick, not the facing |
| the turn spring (0x211e30) | `ovl_21.cpp` | `advanced_turn_target` | while strafing, the target heading is the camera's |
| `hero_vertical_velocity` (0x214108) | `ovl_21.cpp` | `advanced_rac3_movement` | the double jump's kick always applied; the jump's rising time from the state's own ramp |
| `hero_horizontal_velocity` (0x214658) | `ovl_21.cpp` | `advanced_strafe_flip_active`, `_speed` | the strafe flip's speed; stronger air braking |
| `run_update` (0x217970, the run state's block) | `hero_state_update.cpp` | `advanced_rac3_along_stick`, `advanced_rac3_run_turn` | when to move along the stick; the turn springs; UYA's acceleration and deceleration |
| `enter_jump` (in 0x2223f8) | `hero_state.cpp` | `advanced_rac3_jump` | take-off frame, heights, air speeds |
| `run_jump`, `run_input`, `swing_input` (in 0x229b70) | `hero_transitions.cpp` | `advanced_rac3_running_jump`, `advanced_rac3_stop`, `advanced_strafe_flip`, `advanced_rac3_swing_exit` | the running jump's rule, the immediate stop, the strafe flip, running out of a swing |
| `moby_skin_all` (0x2558e0) | `moby_skin.cpp` | `advanced_pose_hook` | the strafe's lower-body twist (below) |
| `gameplay_frame` (0x297f78) | `game_frame.cpp` | `advanced_frame`, `advanced_aim_before_hero` / `_after_hero`, `advanced_aim_before_camera` / `_after_camera` | the combo table, the legs, the aim and the camera around the hero's and the camera's updates |
| the weapons' hand updates (0x2b77f0, 0x2b2100, 0x2cc608, 0x2b87e0, 0x2bc7e8, 0x2da790 / 0x2ddf60 / 0x2ddcb8, 0x2b3058, 0x2d2158) | `pyrocitor.cpp`; the `host` entries of `tools/codegen/asm2fn_specs/*.json` | `advanced_aim_shot`, `_shot_hand`, `_frame_hand`, `advanced_aim_angles`, `advanced_aiming` | with modern aiming, the shot's direction (or frame, or yaw / pitch) on the reticle instead of the hero's facing |

Novalis, Aridia and Kerwan run their own build of the hero's state code (the `*_l1.cpp` files); the
same hooks are in those copies.

### The strafe

With Movement = RAC 3, holding **L2 or R2** on the ground or in a jump makes Ratchet strafe, as in UYA
(UYA's hero tick tests the same two buttons): he keeps facing the camera's heading and moves along
the stick in any direction at full analog speed. The game no longer sees L2 and R2 as such; L1 keeps
the first-person view and R1 the crouch. The right stick still turns the camera, and Ratchet turns
with it.

What comes from UYA and what is this project's own:

- **From UYA:** the buttons, moving along the stick at full speed, no long running jump while
  strafing, and the **strafe flip**: Cross with the stick pushed past 0.9 sideways or back performs a
  flip along the stick (R&C1's flip state 0xb, its animation chosen by the stick's quadrant). As in
  UYA it starts at 5 units/s and approaches max(5.5 x tilt, 2) units/s, along the stick only, taking
  off one frame after the press; R&C1's own flip starts from 0 and covers about 3.2 units, UYA's about
  5.2.
- **This project's own:** how Ratchet is held on the camera's heading (a stiffer turn spring than the
  crouch walk's, snapping onto the heading when close; UYA's own spring values were not used), and
  turning him while he stands still.
- **The legs, invented because R&C1 has no strafe animations.** R&C1 has no sideways or backwards run
  clips (its clips 0x1c to 0x1f are the flips), and UYA's clips are another disc's data. So while
  strafing, the run's animation is reused:
  - **sideways**, after the game has posed the skeleton (`moby_skin_all`, 0x2558e0),
    `advanced_pose_hook` turns the joint matrices of the pelvis (joint 1), the legs and the tail
    (joints 92 to 110 of Ratchet's 111) about the vertical axis toward the direction of movement, by
    an angle eased at 0.15 radians a frame, so the chest, arms and head stay on the aim;
  - **backwards** (more than 2 radians from the facing), the run's frames are shown in reverse
    (`backpedal_show` after the hero's update) and the real frames put back before the next update
    (`backpedal_restore`), so the game's own animation stepping goes on forward. The legs are turned
    onto the backward line.

### Modern aiming

Aiming = Modern (it needs Movement = RAC 3) is a modern shooter's aim, invented for this port, built
on the strafe:

- **L2 aims** (strafing as above), and **R2 fires**: the port turns it into Circle, the original's
  fire button, which still works. In the menus nothing changes.
- **The right stick turns the aim**, not the camera: its heading, and its pitch between looking down
  0.75 and up 0.55 radians, held where it is let go. It follows the camera's own invert options.
  With the mouse in Direct mode (Options > Controls, #117) the mouse turns the aim by its movement as
  it is, at the Aim Sensitivity (see [Input](input.md#how-the-host-builds-the-packet)).
- **A view over the right shoulder** eases in over 8 frames: 3 units behind a point 1.2 above
  Ratchet, 0.75 to the right. A collision test with the follow camera's own flags keeps it out of
  walls. Only the view the renderer reads is moved; the follow camera's own state is turned onto the
  aim once, when the aim ends, and the view is held on the aim until the camera catches up, so there
  is no jolt.
- **A reticle** at the screen's centre for every item but the wrench, through the game's own camera
  effect (`cam_effect_add`); the Blaster draws its own.
- **Shots along the screen's centre**, up and down as well as sideways. The Blaster's shots aim at
  the first thing the screen-centre ray meets within 80 units (`advanced_aim_direction`). The other
  weapons that fire along the hero's facing take the same line, from their hand, in place of the
  facing: the Pyrocitor's flame (`pyrocitor.cpp`), the Devastator's rockets, the R.Y.N.O.'s
  missiles (both still home on their lock), the Tesla Claw's arc, the Morph-o-Ray's beam and the
  Suck Cannon's shot (`advanced_aim_shot` / `_shot_hand`), the Suck Cannon's suction cone
  (`advanced_aim_frame_hand`) and the Visibomb's launch (`advanced_aim_angles`), through host hooks in
  the generated ports (the `host` entries of `tools/codegen/asm2fn_specs/devastator.json`, `ryno.json`,
  `tesla.json`, `morph.json`, `suck_cannon.json`, `visibomb.json`). The gloves (Bomb, Mine, Doom,
  Decoy) reuse the original's own "aim along the camera" path, which R&C1 already takes when L1 or L2
  is held (`advanced_camera_aim` in `bomb_glove.cpp`, `mine_glove.cpp`, `doom_glove.cpp`; host hooks
  in `decoy.json`), with the aim held through their standing throw (state 0x23) so the camera, and
  with it the throw, is still on the reticle when the glove lets go. As in the original, leaning on a
  wall keeps the hand's own direction and the first-person view the camera's. Left as they are: the
  Taunter (its reach is flat: horizontal distances and a horizontal cone), the Walloper (a punch),
  the Swingshot (it locks onto swing points), the Drone Device (no hand code: the drones pick their
  targets), the Hydrodisplacer and the Metal Detector (no direction).

### Modern, No Zoom (#70)

Aiming = **Modern, No Zoom** is Modern aiming without the view over the shoulder: the lock-strafe of
*Ratchet & Clank 3* and *Deadlocked*. L2 aims and R2 fires, the right stick (or the mouse) turns the
aim's heading and pitch, the reticle is at the screen's centre and the shots go along it, as with
Modern; but the view stays at the follow camera's own distance, behind and above Ratchet, held on the
aim's heading and turned about the point 1.2 above him for the pitch (`aim_shoulder` is given no
shoulder weight: only its wall test runs). The screen's centre is above Ratchet's head there, so the
reticle is not hidden by him. It is the Aiming byte's third value (2): everything that tests "Modern"
(the trigger mapping, the pitch, the reticle, the shots) takes it too.

### What the RAC 3 movement measured

In the project's automated runs: top running speed 6.17 units/s (Original 5.7), 95 % of
it in 12 frames (46), a stop in 8 frames (29), lift-off 3 frames after the press (7).

## Limits

- **Not taken from UYA:** its quick-turn triggers, idle braking, crouch values and crouch high jump,
  its strafe facing constants, the lock-strafe while firing, and its strafe flip's quadrant mapping
  (the flip uses R&C1's own quadrants).
- When backing into a wall while aiming, the camera can come very close.
- The page's French texts are shown in English.
- Buy All Ammo: the entry's hologram, the monitor's picture and the ammo counter at the top left show
  one weapon of the list (the one not full with the cheapest round), and after a purchase the counter
  can keep showing the weapon whose ammo was added last until the cursor moves.
