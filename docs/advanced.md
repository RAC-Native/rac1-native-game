# Advanced options

Options > Advanced holds the settings that change how the game plays. The pause menu's page holds a playthrough's
settings (the rows below, set first by New Game's Original / Modern and kept with the save); the main menu's holds only
the global ones, settings of the whole game rather than of a playthrough: [Cut Content](#cut-content),
[Corrected Texts](#corrected-texts), [1.02 Fixes](#102-fixes) and [1.02 Gameplay](#102-gameplay). Beside it, the main
menu's Options has a page of its own for fixes of the original game's own bugs, [Bug Fixes](#bug-fixes). They are
different in kind
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
| Quick Select Rings | 1, 3 | enhancement |
| Aiming | Original, Modern, Modern, No Zoom | enhancement |
| Movement | Original, Strafing, RAC 3 | enhancement |
| Fades | Fast, Original | a removed compromise: Fast is the port's default, Original the PS2's |
| Buy All Ammo | Off, On | enhancement |
| Vibration | Off, Low, High | enhancement, experimental (kept as the machine's preference, not with the saves) |
| Adaptive Triggers | Off, On | enhancement, experimental, shown only when the pad in use is a DualSense (kept as the machine's preference) |
| Skip Cutscenes | Original, Always | enhancement |

All of it is in `src/game/enhance/advanced.{h,cpp}` (Buy All Ammo's Vendor entry in
`src/game/enhance/vendor_buy_all.{h,cpp}`). None of it is a port: every function there is annotated
`// orig: (none) — enhancement`. The Voice row that was last on this page is now on the Language page, with the text
language and the subtitles ([Settings](settings.md#language)); its line `voice_language` stays in the same
preferences file.

## How the page is built

The page is made from the game's own menu data, so it looks and behaves like the game's pages. When
the pause menu opens (`menu_open`, 0x276190, calls `advanced_menu_install`), the port copies the
original's Camera page (0x1b5a60), its title and its "settings" widget (the widget at 0x27ef00 whose
rows are a name, the byte it sets and up to four value texts; Cross cycles the value) into a block
of EE memory the game never uses (0xf0000 to 0xf2000, its texts at 0xfb000 to 0xfe000, inside the PS2 kernel's first megabyte, which
the port does not use since its kernel is the host). It then writes its own rows and texts there and
inserts Language (`language.cpp`, the same way: its block 0xfa200 to 0xfa600, its texts 0x7fc0 and up), Graphics,
Controls and Advanced into the Options list before Quit Game; in the main menu the game's own Language entry opens our
Language page instead of the game's list. The main menu gets
the same pages (`title_menu_page_start`), but its Advanced page is built with the global rows only (a row's `global`
flag in `SETTINGS_ROWS`), and its Options list gets the Advanced entry only when there is one (`advanced_main_page`). The game's text lookup (0x203ad8) asks `advanced_text`
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
  `update_channel`, and `update_skip`, the version the update notice's Skip Version chose), nor are
  Vibration and Adaptive Triggers (the machine's pad).

The ids in `save<N>.pc`: Quick Select 1, Quick Select Rings 16, Aiming 2, Movement 3, Fades 4, Buy All Ammo 5, Skip Cutscenes 10 (Vibration 6,
Adaptive Triggers 9 have ids but are not written with the saves; 7 and 8 are removed rows, and 11 was the Voice row,
now on the Language page, never written with the saves; 12, Cut Content, 13, Corrected Texts, 14, 1.02 Fixes, and 15,
1.02 Gameplay, are global and never written with the saves either: none reused).

**The Language page's rows** have more values than the widget's four: each byte is a step byte with two values whose
texts are both the row's value, made at each lookup (as the Graphics page's rows); when Cross or left / right step it,
the setting moves on and the byte goes back to 0. Voice is kept as `voice_language same|en|fr|de|es|it|ja`, Subtitles as
`subtitles on|off` (only once chosen), Movie Subtitles as `movie_subtitles on|off` in the preferences, the text language as the console's (`console.txt`). What they
change is in [Settings](settings.md#language); New Game's modes leave them as they are.

A preferences file or a `save<N>.pc` written by an earlier build may hold a setting that has since been
removed (`ship` and `clank`, the ship selector and Clank's movement, ids 7 and 8): it loads, and those
lines are ignored.

**The New Game choice.** Picking "Modern" in New Game's first window turns Quick Select, Quick Select Rings (3), Aiming
(Modern), Movement (RAC 3), Buy All Ammo, Vibration (High), Skip Cutscenes (Always) and, when the pad in use is a
DualSense, Adaptive Triggers on; "Original" turns them all off
(`advanced_apply_mode`). Fades and the global rows are left as they are. Every setting can be changed at any time
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

## Quick Select Rings: 1 or 3

With 3, the quick select has three rings of eight places, as in the later games: ring 1 is the game's own, rings 2 and 3
are extra. At 1 (the default, and New Game's Original) the game is the original's; New Game's Modern turns it to 3.

- **In game**, while the ring is open (Triangle held): **L1 or L2** shows the ring on the left, **R1 or R2** the ring on
  the right. They wrap (left of ring 1 is ring 3). Those presses never reach the game: no crouch, no first-person view,
  no Modern aiming's fire, until the button is let go (a button held before the ring opened stays the game's). Above
  the ring: the L1 / R1 arrows (the pause page's frame 0xe99e and the font's L1 / R1 glyphs) and three dots, the ring
  shown lit; the new ring's icons fade in. The selection stays on its place when the new ring has an item there. The
  stick, the d-pad and the PC's next / previous weapon (mouse wheel, X) choose inside the ring shown; letting go of
  Triangle equips from it, as the game does (gadgets too: the Drone Device's own request).
- **Which ring opens**: the one shown when the quick select last closed. Closed, the PC's next / previous weapon steps
  through that ring.
- **Pause menu > Quick Select**: L1 / R1 still turn the selected place; **L2 / R2** change the ring being edited, shown
  in the wheel's centre ("Ring 2", its dots under it) with the L2 / R2 arrows in the page's bottom corners. The page
  opens on the ring the quick select shows. Cross puts the focused item in the ring edited: the same item may be in more
  than one ring, and placing it takes it only from its old place in that ring.
- **New items**: a weapon the game would put in ring 1's first free place goes, when ring 1 is full, to ring 2's first
  free place, then ring 3's.
- **Mods**: a plugin's `weapon_allow` leaves the weapons it does not allow out of every ring (empty places), as it does
  for ring 1.

**Where they are kept.** Ring 1 stays the game's list 0x141ea0, in the game's own save as before. Rings 2 and 3 and the
ring last used are item 0x400 of `save<N>.pc` (18 bytes: version 1, the ring last used 0..2, then rings 2 and 3, one byte
an item id, 0 empty), written only when there is something in them ([Saves](saves.md)). The original game, PCSX2 or a PS2
read the card as before and never see them. A new game empties them; a save without the item loads with them empty; a
challenge mode's new game keeps them, as it keeps ring 1 (items no longer owned dropped); deleting the save data deletes
the `.pc` files with it.

**How it is built** (`src/game/enhance/quick_rings.cpp`, `src/host/quick_rings.h`; the row's byte in `advanced.cpp`):

- `pad_build` (the pad state the game reads) passes the controller's buttons through `quick_rings_pad` first: with the
  quick select open (the HUD's slot 3 updated by 0x237ba0) a shoulder button's press is noted as a ring switch and taken
  out of the buttons the game sees until it is let go.
- `quick_select_start` (0x237ae8) fills the eight slots from ring 1 as the original; then the ring last used is put in
  them. `quick_select_update` (0x237ba0) switches the ring asked for (the slots' items and icons again, the plugin's
  disallowed items blanked); `quick_select_draw` (0x2382a0) draws the arrows and dots above the box and reads the
  shown ring's item for the ammo and the name. The equip path is the game's: the slot's item.
- `menu_quick_focus_in` / `focus_out` / `update` / `draw` (0x27a610, 0x27a6a8, 0x27a3e8, 0x27e768) read and write the ring
  edited instead of 0x141ea0 when it is ring 2 or 3.
- `item_give` (0x260860) calls `quick_rings_give` when ring 1 had no free place.
- The texts: the row 0x7f22 ("Quick Select Rings"), its values 0x7f23 / 0x7f24 ("1", "3"); "Ring 1".."Ring 3" 0x7a00..0x7a02,
  made at each lookup in the block 0xfa100..0xfa200 (the buttons' glyph strings there too).
- The log: `[quick rings] opened on ring N (in hand 0x..)`, `ring N shown`, `closed on ring N, place P, item 0x..`,
  `shoulder buttons 0x.. kept from the game`, `rings 2 and 3 loaded with the save`.

At 1, and with every enhancement off (the headless tests), none of it runs: a scripted run with the ring opened and the
shoulder buttons pressed, in game and on the pause page, gives the same RAM, frame by frame, as the build before it but for
the Advanced page's own block (its new row and texts).

## Vibration (#10)

> **Vibration and Adaptive Triggers are an experiment**: our own choice of events and strengths, which may still change a
> lot. They do not reproduce any other game's vibration.

The original has no vibration. With **Vibration = Low** or **High**, the
gamepad answers what happens: our own pulses for the game's events. Each event is a short pulse
with a crisp end: held at its strength for about half its length, then brought down to 0 in a few
frames. The strong, low-frequency motor (the left one) takes impacts and explosions; the weak,
high-frequency one (the right one) takes shots and ticks. Low is a little under half of High. Vibration
is off by default and in Original mode; Modern sets it to High.

| Event | Low motor | High motor | Length | Felt from its side (DualSense) |
|---|---|---|---|---|
| A shot: Blaster, Pyrocitor, Tesla Claw | 0 | 0.35 | 50 ms | no |
| A shot: the gloves and the other weapons | 0.25 | 0.45 | 70 ms | no |
| A shot: Devastator, R.Y.N.O. | 0.7 | 0.5 | 90 ms | no |
| The wrench's hit | 0.3 | 0.5 | 60 ms | no |
| A hit taken (1 nanotech; 2 or more) | 0.6 (0.8) | 0.41 (0.55) | 167 ms (to 250 ms) | yes: the attacker |
| A hit on the ship's shield | 0.55 | 0.4 | 150 ms | yes: the attacker |
| An explosion, by its distance (k = 0.15 far .. 1 near) | k | 0.5 k | 200 to 300 ms | yes: the blast's sound |
| A hard landing | up to 0.6 | up to 0.25 | 100 ms | no |
| Death | 1.0 | 0.8 | 600 ms | no |
| Bolts picked up (at most every 4 frames) | 0 | 0.12 | 33 ms | no |
| The ship flying | 0.12 | 0.04 | while it flies | no |
| The thrown wrench | 0 | 0.1 | while it flies | yes |
| A moving sound source (a flying enemy, a ship) within 20 units | up to 0.12 | up to 0.03 | while it moves | yes |

Lengths are in gameplay frames (60 a second), rounded: 50 ms is 3 frames. Pulses that overlap take the
stronger of each motor; the motors are at exactly 0 on the frame the last pulse ends.

**The game is only read.** Once a gameplay frame `vibration_frame` (`src/game/enhance/vibration.cpp`,
called by `advanced_frame`) compares what the game's own code did with the frame before: the nanotech
(+0x22a8) and the hits counted by `hero_hurt` (0x15eea8), with the attacker it keeps (+0x2280), the hero's
state (the death states 0x3d and 0x5d) and the lost-hero flag (+0x20b1), the fall speed when the jump
states end, a new camera shake (0x166de0 / 0x166de8, which the weapons' blasts, mines and bombs set when
they go off in view, 0.4 - 0.0175 x the distance), the ammo of the item in hand (0x13d428 + item x 4), the
wrench swing's hit flag (+0xa64, set by `wrench_hit_sparks` 0x217368), the bolt count (0x15ed98), the ship
mode (+0x20a4 = 2), the thrown wrench (+0x10aa, its moby +0x1090) and the game's 30 sound slots
(0x13e5c0). Reading the results instead of hooking the functions covers every level's own build of the
hero and weapon code (several levels run transliterated copies) with one reader, and cannot change the
game: a run with Vibration on has the same RAM hashes as one without, outside the menus' own block. The
host takes the result once a game frame (`vibration_take`): neutral when no gameplay frame ran, so menus,
pauses and a level's loading still the motors and give the triggers and the light bar back.

**From its side.** An event with a position is felt from where it is on screen: its direction in the
camera's view (the camera's position 0x166dc0 and its rows 0x166fd0: forward, left, up) gives a pan from
-1 (left) to 1 (right), centred within a unit of the camera, as the game's own sound pan (0x28c9e8) is.
- An explosion's position is the sound the blast started that frame: of the sound slots new that frame,
  the one whose distance from Ratchet is nearest the distance the shake's size says. None close enough
  (a shake that is not a blast's): the pulse stays centred.
- A hit's is the attacker's moby, which `hero_hurt` keeps in +0x2280.
- The thrown wrench, and a moby's sound playing for a second or more while the moby moves faster than
  9 units a second within 20 units (the flying enemies' engines, the ships around Ratchet), are followed
  each frame: their hum moves around the player. Only the nearest one is felt.
On a **DualSense** the two actuators are a true left and right: a positional pulse of strength `a` is split
with a constant-power pan (`a * min(1, sqrt2 * cos t)` left and `a * min(1, sqrt2 * sin t)` right, t from 0
to a quarter turn), so in the centre both sides have the full strength and to one side the other fades
out. On the **other pads** the two motors are the low and the high frequencies, not left and right, so
the pulses stay centred there.

**Adaptive Triggers** (experimental, like Vibration; shown only when the pad in use is a DualSense; off by default; Modern
turns it on when a DualSense is in use) works with Aiming = Modern, where L2 aims and R2 fires,
in gameplay on foot:
- L2: a soft resistance from a quarter of its travel;
- R2, by the item in hand: the Blaster, the Pyrocitor and the Tesla Claw a light resistance that rattles
  while they fire; the gloves and the other weapons a stiff section that gives way (a click); the
  Devastator and the R.Y.N.O. a harder one; each shot a short kick; the wrench and the gadgets nothing;
- the light bar: Ratchet's health, from green (full) through yellow to red.
The triggers and the light bar go back to neutral (no effect, the pad's own colour) in menus, in pauses,
during a level's loading, when the setting goes off and when the game ends. With Aiming = Original the
triggers do not fire, so they stay neutral; the light bar still follows the health.

**The pad** (`src/host/rumble.cpp`, `src/host/dualsense.cpp`). GLFW has no force feedback, and no library
is added for it:
- A DualSense, on Linux and Windows: the game writes the pad's own output report, in the layout of the
  kernel's `hid-playstation` and SDL: report 0x02 over USB, 0x31 over Bluetooth (with its sequence number
  and a CRC-32 of 0xa2 and the report). It sets the two actuators (as the kernel's rumble does, in the
  firmware's newer rumble mode from 2.21 on, read from feature report 0x20), the triggers' effects (the
  simple ones: 0x01 resistance, 0x02 section, 0x06 vibration, 0x05 off) and the light bar. Linux: the
  pad's `/dev/hidraw*` node, found by the bus, vendor and product of GLFW's GUID; Steam's and the
  systems' udev rules usually give the seated user access to it. Without access the log says why, and
  the kernel's force feedback below is used instead (rumble only, no triggers). Windows: the HID device
  (SetupAPI, `hid.dll`), written with its own report length. `tests/rumble/dualsense_test.cpp` checks the
  layout and the CRC (`cmake --build build --target dualsense_test && ./build/dualsense_test`).
- Linux, other pads: the kernel's force feedback (`FF_RUMBLE`) on the pad's event device, found among
  `/dev/input/event*` by the same bus / vendor / product / version numbers GLFW's GUID is made of. The
  kernel's drivers cover the Xbox pads (`xpad`), the DualShock 4 (`hid-sony`, `hid-playstation`), the
  Switch Pro (`hid-nintendo`) and most others. One effect is uploaded once and updated in place; each
  setting is played again, and the motors are stopped by setting 0, playing it, then stopping it. (Until
  this was fixed the stop came right after the upload of the 0: the kernel's `ff-memless`, behind most
  pad drivers, then sends nothing to the pad, which kept the last, weakest strength of the fade until
  the next event: the endless tail.) The desktop gives the seated user access to a gamepad's event
  device; without it the log says so and nothing vibrates.
- Windows, other pads: XInput (`XInputSetState`, from `xinput1_4.dll` or `xinput9_1_0.dll`, loaded when
  first needed) on every connected Xbox-compatible pad, and a DualShock 4 or DualSense that Steam Input or
  DS4Windows presents as one.
- macOS: none.
The pad driven is the one last used (the one the Controls page names). A force feedback setting lasts one
second and is renewed every half second while the motors run, and the host stills the pad when the game
gives nothing for 150 ms, so a stalled game never leaves the pad buzzing.

`RAC1_VIBRATION_LOG` prints each event's pulse (its motors, length, pan, distance), the motors and the
triggers when they change (`[vibe] f<gameplay frame> ...`) and what the host takes (`[vibe] host frame
...`), with or without a pad. `RAC1_DUALSENSE_OUT=<file>` takes a DualSense as connected (in headless runs
too) and appends its output reports to the file instead of a device.

## Skip Cutscenes: Original or Always

**The game's rule.** The game has one skip test for its in-engine scenes (game mode 2, `cutscene_update`
0x298f90) and the same one for its pre-rendered movies played in a level (the Infobots, the story movies, the help
desks' videos: the movie player's loop, its test at 0x2eddd0):

- **Start alone** skips on Veldin (level 0), in Challenge Mode (the count 0x15ee20 set), and with 0x15eea0 or
  0x15eed8 set (0x15eea0 is never written; 0x15eed8 is 2 for the movies the pause menu and the title play, which any
  button skips);
- **everywhere else**, only **L1 + L2 + R1 + R2 held with Start pressed** skips (a hidden combination);
- an in-engine scene takes neither before its 18th frame or while its fade-in is still on.

The boot's intro movie is never skippable (0x15eed8 = -1). The planet arrival and departure scenes (game mode 6) take
Cross or Circle after 30 frames on every planet, and the help desk's messages Triangle: neither has a condition to
lift. The flight between planets is the loading screen and ends with the level's loading.

**Always.** With **Skip Cutscenes = Always**, the condition passes on every level: Start alone skips every in-engine
scene and every movie, the boot's intro too. The skip itself is the game's own, the one Veldin and Challenge Mode run:
a scene's skip goes to `cutscene_end` (0x298b18), the same end as a scene played to its last frame (the hero given
back and placed, the scene's mobys removed, the talk script stepped to its next record), and a movie's sets the
player's stop flag, then `movie_mode_end` (0x2997c8) continues the talk. Items, story flags and the Infobots' new
planets come from the talk scripts and the classes' own updates after those ends, so a skipped scene gives what a
watched one gives. Challenge Mode's own count is not touched (it also doubles the bolts). Original leaves the game's
rule as it is.

The two tests (`src/game/ovl/cutscene.cpp`, `src/game/ovl/movies.cpp`) ask `advanced_skip_cutscenes()`, false while
every enhancement is off (headless runs, the oracle).

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
screens (`src/game/ovl/vendor_*.cpp`, each telling its call site): the tile's
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

On the hoverboard (Rilgar's and Kalebo III's board, the hero states 0x6b..0x6f) L2 and R2 are the game's own with
either value (#28): they are two of the board's four tricks (R1, L1, R2, L2), and the skill point Tricky (the Twisty
McMarx: all four in one jump) needs them. Up to 0.7.0 the strafe took them there too (and Aiming = Modern turned R2
into circle), so only the R1 and L1 tricks could be done and Tricky could not be earned.

Clank (and Giant Clank) strafe too (#129): with Movement = Strafing or RAC 3, L2 held (L2 or R2 with Aiming =
Original) keeps him facing the camera's heading while the left stick moves him in any direction, standing,
walking and jumping (his stand 0x43, walk 0x44 / 0x50 and jumps 0x49 / 0x4c; Giant Clank's 0x5a, 0x5b, 0x5e:
`aim_state`), and the right stick turns the aim and him with it; with Aiming = Modern (or Modern, No Zoom) the view is
the aim's, as Ratchet's, and R2 is circle (Giant Clank's missiles). His attacks (square: the kick 0x51, Giant Clank's
punches 0x60) go the way he faces, the aim's heading; the strafe pauses during them, as during Ratchet's wrench
swing. There is no reticle (he has no weapon that aims at it), L1 is still his first-person view, and the Hologuise
(form 3) plays as the original. His walks cut their acceleration by the angle between his facing and the stick (to
almost nothing backwards), which makes no sense while the facing is held: strafing, the acceleration is the walk's
full one (`advanced_strafe_accel`, in 0x212088, the step every build of the hero code calls). Up to 0.7.0 the strafe
and the modern aim turned on in Clank's jump only, since only it was among the strafe's states.

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
| the weapons' hand updates (0x2b77f0, 0x2b2100, 0x2cc608, 0x2b87e0, 0x2bc7e8, 0x2da790 / 0x2ddf60 / 0x2ddcb8, 0x2b3058, 0x2d2158) | `pyrocitor.cpp`, `devastator.cpp`, `ryno.cpp`, `tesla.cpp`, `morph.cpp`, `suck_cannon.cpp`, `visibomb.cpp`, `decoy.cpp` | `advanced_aim_shot`, `_shot_hand`, `_frame_hand`, `advanced_aim_angles`, `advanced_aiming` | with modern aiming, the shot's direction (or frame, or yaw / pitch) on the reticle instead of the hero's facing |

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
  (`advanced_aim_frame_hand`) and the Visibomb's launch (`advanced_aim_angles`), through calls in
  their hand-written ports (`devastator.cpp`, `ryno.cpp`, `tesla.cpp`, `morph.cpp`, `suck_cannon.cpp`,
  `visibomb.cpp`). The gloves (Bomb, Mine, Doom, Decoy) reuse the original's own "aim along the
  camera" path, which R&C1 already takes when L1 or L2 is held (`advanced_camera_aim` in
  `bomb_glove.cpp`, `mine_glove.cpp`, `doom_glove.cpp`, `decoy.cpp`), with the aim held through
  their standing throw (state 0x23) so the camera, and with it the throw, is still on the reticle
  when the glove lets go. As in the original, leaning on a wall keeps the hand's own direction and
  the first-person view the camera's. Left as they are: the Taunter (its reach is flat: horizontal
  distances and a horizontal cone), the Walloper (a punch), the Swingshot (it locks onto swing
  points), the Drone Device (no hand code: the drones pick their targets), the Hydrodisplacer and
  the Metal Detector (no direction).

### Modern, No Zoom (#70)

Aiming = **Modern, No Zoom** is Modern aiming without the view over the shoulder: the lock-strafe of
*Ratchet & Clank 3* and *Deadlocked*. L2 aims and R2 fires, the right stick (or the mouse) turns the
aim's heading and pitch, the reticle is at the screen's centre and the shots go along it, as with
Modern; but the view stays at the follow camera's own distance, behind and above Ratchet, held on the
aim's heading and turned about the point 1.2 above him for the pitch (`aim_shoulder` is given no
shoulder weight: only its wall test runs). The screen's centre is above Ratchet's head there, so the
reticle is not hidden by him. It is the Aiming byte's third value (2): everything that tests "Modern"
(the trigger mapping, the pitch, the reticle, the shots) takes it too.

### On magnetic surfaces (#136)

On a magnetic surface with the Magneboots (its collision's surface type 2 under the boots, `+0x308`, or a ground mode of
its own, `+0x20b3`: the magnetic walk 0x3f and its stance / jump 0x70 / 0x71) the hero's frame is the surface's: his
heading is measured in it and "horizontal" lies along it. The strafe, the modern aim and R&C 3's movement are made for
the world's up (the aim's heading spring writes a world heading, the camera is held on the aim's heading and pitch about
the world's up, R&C 3's stop caps the world's x / y), so there they are not applied (`on_magnet` in `advanced.cpp`,
tested by `advanced_strafing` and `advanced_rac3_movement`): the original's own rules and camera hold, L2 does nothing
(the original's first-person view stays on L1), and the aim comes back when he steps off. Before, holding L2 on a
magnetic wall turned the hero's whole frame off the wall (measured on Orxon's tube: his angles +0x90 went from -1.37 to
+1.42 radians while standing) and held the camera on the aim as on flat ground.

### What the RAC 3 movement measured

In the project's automated runs: top running speed 6.17 units/s (Original 5.7), 95 % of
it in 12 frames (46), a stop in 8 frames (29), lift-off 3 frames after the press (7).

## Cut Content

The main menu's Advanced page has one row, **Cut Content: Off / On** (off by default). It is a global setting: kept in
the preferences only (`cut_content 0|1` in `advanced_options.txt`), never with a save, and New Game leaves it as it is.
With it on, things the retail game carries on the disc but never shows come back. Each one's own code in the game is a
stub, so its behaviour is reconstructed, or ported from an earlier build that still has it
(`src/game/enhance/cut_content.{h,cpp}`, functions chosen where the game calls the original's): with the row off, the faithful port runs, and the game's memory is the same frame by
frame (RAC1_RAMHASH). The row is read when a level's mobys first update, right after its load: a change shows at the next
level load. `RAC1_CUT_CONTENT=fred,...` turns single items on for testing ([Environment variables](env-vars.md)).

| Item | Level | What comes back |
|---|---|---|
| `fred`: Kerwan's citizens "Fred 1 / 2 / 3" | Kerwan | classes 915, 916, 917, 14 placements in the streets, on stairs and in a tunnel (x 204 to 350, y 130 to 202) |
| `ships`: Eudora's Transport Ships | Eudora | class 484, 3 placements; each brings in the enemies placed for it (4 + 3 Circular Saw Enemies, 3 Blarg Troopers) |

**Kerwan's citizens.** Their shared update (L3:0x2dc310, `fred_update_l3`) measures the distance to Ratchet and the
heading towards him against its own, throws both away and retires the moby on its first frame; the prototype build already
has the stub. With the item on, a host update replaces it (`cut_content_fred_dispatch_l3`, registered at that address):
the citizen stays, an idle is played in a loop (below), and within 15 units it turns to face Ratchet
(`yaw_turn_damped`, the game's turn, slow). What was learnt bringing them back:

- Each of the three models has one sequence of **one frame**: the rest pose, arms held out (#128, "they T-pose like
  cryptids"); Kerwan's data has nothing else for them. They play **another citizen's idle** instead: Nebula G34's
  "Fred" (level 6, class 1105) has Fred 3's skeleton (65 joints, the same hierarchy) and Fred 1 and 2's but for their
  six extra joints under the head (55..60); its frames' rotations equal the Kerwan models' rest pose everywhere but the
  arms, so they carry over (its bones are longer: each model keeps its own lengths). That citizen is the grind boots
  seller ("Fred Prime"). Of his two sequences, 0 is a stiff standing idle (50 frames: only the head and a shoulder move;
  it was used up to 0.7.x, #136) and 1 is the one his own update plays while he waits: crouched, the hands together at the
  chest (15 frames at 0.25 a game frame, 1 s, a loop), with the hips' translation (joint 4) in each frame. Sequence 1
  is used (#136): the models' root stays at their standing height, so each citizen is lowered by 0.25 to keep its feet on
  the ground. It is read from the player's disc (level 6's core) at the first citizen kept on a level, rebuilt on each
  class's joints (Fred 1 and 2's extra joints keep their rest rotations) in host memory above the game's 32 MB
  (`host_layout::CUT_CONTENT_SEQS`) and put in the class's sequence slot 0; each citizen starts it at its own frame. Their
  placements' flags keep the game's moby loop from advancing their animation (+0x34 bit 0x40), so their update advances
  it. Nothing of the sequence is in the repository; without it (no level 6 data) they keep the rest pose. The other
  humanoid NPCs of the game do not fit: Nebula G34's Fred and Quartu's Ultra-Mech Scientist (1446, the same 65 joints,
  one 18-frame gesture) are the only ones with this skeleton; no NPC class is a Gadgetron engineer; the Gadgetron NPCs (the
  CEO, 1455, 90 joints; the Help Desk Lady, 1377, 62) and the other NPCs have other skeletons (14 to 99 joints).
- They have **no collision**: Ratchet dropped on each one's spot lands where he lands without them, so none of the 14
  blocks a path, even those standing on a path, a stair or in the tunnel.
- Some **placements stand in the air**: three (x 314 to 323, y 139 to 146) 0.25 above the ground, four more 0.03 to 0.1
  (the game never showed them). At its first update each citizen is put on the ground under it: the game's own segment
  test (`segment_collide`) from 1 above its placement down to 8 below, the citizen's height set to the hit point; with
  nothing hit it stays as placed (all 14 hit). With the item off nothing changes (the faithful stub runs).
- Their turn's spin is kept in the last word of their pvars (448 bytes of placement data no code of the game reads);
  the state byte (+0x20) marks a citizen kept. With the idle, the game's memory differs besides in the three classes'
  sequence slots (their headers) and the frame's graphics chains (the poses drawn). No save data is touched: over a run through Novalis, Kerwan, Rilgar and
  Batalia, the game's memory differs only from the citizens' first update on Kerwan, in Kerwan's level data and moby
  pages; after the next level's load only pages of Kerwan's data the next level does not overwrite differ, and the game's
  state (the hero, the items, the save's blocks) stays the same.

**Eudora's Transport Ships.** Retail's update for class 484 (L4:0x2ce060) is `moby_retire`: the three ships placed on
Eudora go on their first frame, and the enemies placed for them stand where they were put. The beta (August 2002) and
prototype (June 2002) builds keep the real update, 1872 / 1896 bytes; with the item on it runs, ported from the beta
(`cut_content_ships_update_l4`, `// orig: beta 2002-08 L4:0x002a18e8`, registered at the retail address through
`cut_content_ships_dispatch_l4`). What it does, from its code and the placements' data (their pvars have the beta's
layout, 384 bytes):

- Each ship waits hidden, with the enemies it carries (up to four, moby indexes in its pvars +0x140..+0x14c) hidden
  under it, until Ratchet enters its trigger box (`point_in_box`, pvars +0x150). If the level's collected-things row
  (0x14c050, the ship's entry +0xb0) already marks it done, the ship and what it carries retire at once.
- Then it shows and flies its path (pvars +0x74) through the shared ship update of the other Eudora ships
  (L4:0x2ca420, `eud_class466_update_l4`: retail's still has its class 484 case), the enemies held at four places in its
  frame (a table retail still has, L4:0x1dc320, read there). From the first point of its path whose w is 1 it drops
  them one by one, every 5 to 8 frames, facing Ratchet, in their own drop state (state 2 for the Circular Saw Enemy 340,
  4 for the Blarg Trooper 427: retail's updates still have both); two points before its path's end it retires.
- Two of the three start a short scripted camera when they come (a camera box of the level, pvars +0x160, for +0x168
  seconds; the camera turns to follow the ship within +0x170 degrees), Ratchet held still meanwhile (hero state 0x1f,
  back to idle after).

| Ship | Placed at | Trigger box | Path, drop point | Carries | Camera |
|---|---|---|---|---|---|
| 1 | 237, 249, 61 | 12 (260, 243, 58) | path 8, drops from point 5 near (255, 257) | 4 Circular Saw Enemies (placed at 252..263, 254..258) | box 19, 4 s, 20 degrees |
| 2 | 324, 306, 67 | 35 (292, 289, 67) | path 73, from point 6 near (311, 277) | 3 Circular Saw Enemies (305..312, 274..277) | box 34, 3 s |
| 3 | 288, 274, 65 | 35, the same | path 72, from point 3 near (303, 282) | 3 Blarg Troopers (300..308, 283..287) | none |

What retail lacks and how it is covered: the beta's callees are all in retail, matched by the overlay maps or by their
code (`hero_set_state` 0x2223f8, `camera_scripted_set` 0x2eaaa0, `uid5_set_pvar_90` 0x2eaa30, `moby_anim_advance`
0x250480, the beta's 0x200730 / 0x2bcf38 / 0x2bce68 / 0x2282d8); no sound of its own; the paths, boxes and camera boxes
are retail's level data. One field moved: the Blarg Trooper's gun moby, which the ship retires while it carries the
trooper, is at the trooper's pvars +0xf8 in retail (+0x98 in the beta; the same code around it in both builds' class
427 update). With the item off nothing changes (the faithful retire runs). With it on, until Ratchet comes, the game's
memory differs only in the three ships' mobys and pvars and in the engine's moby lists they stay in; after the next
level's load, which moby slots that level's own spawned mobys take can differ (the ships are left alive instead of
retired, as any Eudora moby left in another state would do).

Left out for now: Orxon's five Fred 1 (class 915 by Clank's start: the same retire stub, L10:0x2d7fe8, not a turning
citizen as once thought - speculative), and the rest of the candidates in [Port inventory](port-inventory.md#what-the-development-repository-already-knew).

## Corrected Texts

The main menu's Advanced page has a row **Corrected Texts: Off / On** (off by default), shown only when the game's disc is
the North American one and a data disc gives the corrected texts: the PAL disc 1.00, the PAL disc 2.00 or the Japanese disc
(see [Setup](setup.md#data-discs)); without one the row is not on the page. It is a global setting: kept in the
preferences only (`corrected_texts 0|1` in `advanced_options.txt`), never with a save. With it on, the strings the later
discs correct are shown as those discs have them, in every language the game shows (English, French, German, Spanish,
Italian). The North American disc's own European tables carry mistakes the PAL disc fixed (described here, not quoted),
for example:

- Italian: the 15 memory card messages name the card's slot in the French wording; the PAL disc names it the Italian
  way, says first that no card is inserted when there is none, and the Load Game prompt's cancel is reworded. Also two
  weapon slogans each shifted to the next weapon, typos, a word's gender, the triangle button spelled out as its glyph.
- Spanish: the map's legend for the sticks is in Italian (the PAL disc shows the sticks' glyphs), and several messages
  join the two names with the Italian conjunction instead of the Spanish one; a card message's English article.
- French: the ammo messages for one unit in the singular, two weapons renamed (the Devastator, the glove), grammar.
- German: a card message that said the wrong thing, a weapon's name in one word, shortened lines.
- English: the Heli-Pack hint names the cross button as its glyph instead of in words; a credits line's breaks.

How (`src/game/enhance/corrected_texts.{h,cpp}`, `// orig: (none)`): at the first text table the game relocates with the
row on, each level's gameplay file of the data disc (`levels/levelNN/gameplay_pal.bin`) is compared with the North
American disc's, string by string by id, in the order PAL 2.00, PAL 1.00, Japanese disc (the first disc that has a
string gives it). About 75 strings differ (6 KB); they are written to an EE block the game never uses (0xfe000 to
0xffc00), and after the game relocates a text table (the title's eight in `title_load`, the level's in `gameplay_load`,
the title's again for the pause menu's help log) the records of those ids point at them. The game's own lookups then
read them as any other string. Details:

- The PAL disc 2.00 added five credits lines (the ids after each one are one higher): its ids are taken back to the North
  American ones, and its credits are not taken (they would need that build's credits code); with only that disc, the
  credits stay the North American ones, with the PAL disc 1.00 or the Japanese disc beside it they get the English line
  breaks fix.
- Spanish 20172: the PAL disc 1.00 broke it (a copy of the line before it), 2.00 fixed it again; it is taken only from
  2.00.
- Still wrong on every disc, so not corrected: a German credits line in English; two Italian misspellings (a weapon's
  name, a word in a hint); two new typos the PAL disc brought in its fixes (one French, one German).
- With the row off nothing is read or changed: the game's memory is the same frame by frame (RAC1_RAMHASH). With it on,
  only the corrected records' string pointers and the block 0xfe000 differ.

The row is read when a text table is loaded, and a change on the main menu acts on the main menu's texts at once (their
original strings put back when it is turned off); a level's texts follow at its load. `RAC1_CORRECTED_TEXTS=1` / `0`
overrides the row for testing ([Environment variables](env-vars.md)).

## 1.02 Fixes

The main menu's Advanced page has a row **1.02 Fixes: Off / On** (off by default), a global setting kept in the
preferences only (`fixes_102 0|1` in `advanced_options.txt`), never with a save. The game's later build, 1.02 (the
Japanese disc and the PAL disc 2.00, the "Platinum"), changed about a hundred functions of each level's copy of the
engine; most of it is the memory card, the Japanese text layouts and recompiled code, and some of it fixes the game.
With the row on, those fixes run on the North American build's code: each one is the 1.02 build's
own change, read from its code (its addresses taken back to the North American ones through
`data/regions/SCES-50916-2.00.txt`) and written into the North American port at the same place, behind its own test
(`fixes102_on(n)`, `src/game/enhance/fixes102.{h,cpp}`; a `(1.02 Fixes, fix n: ...)` note at each site, naming the 1.02
build's function). With the row off the North American port runs, and the game's memory is the same frame by frame
(RAC1_RAMHASH). The fixes that change how the game plays (the RYNO's targets, the Visibomb's steering, the deadly ground,
the interaction guards...) are a row of their own: [1.02 Gameplay](#102-gameplay).

| Fix | What the 1.02 build changed | Where (North American) |
|---|---|---|
| 1 | New Game keeps the sound settings: the effects' and the music's volume and mono / stereo (0x15edf0, 0x15edec, 0x15ede8), which the disc's default save otherwise puts back to its own | `new_game_state` 0x24b628 |
| 2 | Explosions leave moby slots free (0x15ffbc, the free slots): a bomb's ten debris only while 51 slots or more are free, its four others only while 21 are; the generic blast's debris mobys at most the free slots less 10 | `bomb_update` 0x2adb30, `explosion_effect` 0x25e450 |
| 3 | At most 20 Glove of Doom agents: the last 20 kept in a ring, and the agent spawned 20 before, when still an agent and not already dying, goes to its state 4 (it leaves); the ring cleared at each level's load (1.02: 0x1dd400 and 0x16190c; here the EE block 0xffc00, as nothing of the game or the runtime is there) | `doom_agent_spawn` 0x2c8218, `gameplay_load` 0x241940 |
| 5 | The Thruster-Pack's sound on jumps: at frame 5 of a jump (state 0xe) and of the long and wall jumps (0xb, 0xc), with the Thruster-Pack on Ratchet's back, its sound (`hero_sound_start(1, 0)`); not on Oltanis | `hero_vertical_velocity` 0x214108, `hero_horizontal_velocity` 0x214658 (before the levels' own copies) |
| 6 | An animation's next frame (+0x51) kept in its sequence: at most the sequence's frame count less 1 | `hero_anim_set` 0x2325a0 |
| 7 | Ratchet's colour pulse (his moby's +0x90, the light on his wrist; the phase 0x13f350+0x1628) in the cutscenes too: the scene's Ratchet pulses, slower; in play only in game mode 0, and faster, between a second pair of colours, while 0x179410 is set and not 8; in Veldin's opening scene, frames 835 to 1080, the second pair too | `hero_ship_update` 0x234b38, `hero_gadget_glow` 0x2080b0 |
| 11 | The talk prompt: a character's record that waits for the button advances (`talk_step`) only once Ratchet faces it and it faces him, not every frame he is within twice its reach | `talk_update` 0x266448 |
| 15 | The cutscenes: none starts while one already runs (game mode 2); the underwater view (0x167014: the fog and the blue tint of a camera under water) is put aside as the scene starts, so a scene started from the water is not drawn as under water, and put back at its end; the frame's draw lists are emptied at its end (the 1.02 build's copy of `draw_lists_reset`). The 1.02 build keeps the view in the scene's +0x4b; the North American scene block uses +0x46..+0x49 otherwise, so here it is the EE block's word 0xffc54 | `cutscene_start` 0x298840, `cutscene_end` 0x298b18 |
| 16 | The pause menu: the goals updated when the menu comes back to its goals pages; the menu's open counter (MS+0x110) held at 32000 | `menu_page_start` 0x276368, `menu_update` 0x276bd0 |
| 17 | The ship's landing and take-off scenes are skipped with circle too, and its flight to the next planet hurried with circle too (the presses' mask 0x50, triangle / cross, to 0x70). The PAL disc 1.00 has the North American 0x50: a change of the 1.02 build (the Japanese disc's and the PAL disc 2.00's), not of the PAL release | `ship_sequence_update` 0x290590 |
| 18 | Aridia: a sand shark that is killed bursts (`enemy_death_burst`: three sparks of 0.333 and two shards, no blast, 0.333 over its place) before it dives off to its next spot; the original only drops its bolts and vanishes | Aridia's class at L2:0x2d3e50 (`hooks.glue`) |
| 19 | The hoverbike races (Rilgar, Kalebo III): the place ("1st".."5th"), the poses ("x3 Poses") and the flips ("+ 360 Flips") were English built into the code, in every language and always plural; the 1.02 build takes them from the level's text table (in the game's language: "1a", "Posizioni"...), with "Pose" for one and "flip" for a 360 (the texts 21028..21032, 20855 / 20856, 20647 / 20648, all on the North American disc). The PAL disc 1.00 has them too | Rilgar's L5:0x262f50 / 0x266320, Kalebo III's L16:0x235660 / 0x238a30 (`hooks.glue`, `fixes102_race_text`) |
| 20 | Blarg: the class 1083 object that breaks by itself 32 units or more from Ratchet leaves its broken copy but none of its 50 debris pieces (moby slots; the 1.02 build gives the break routine an argument, 0 there). Broken near him, or by him, it is unchanged | Blarg's L6:0x2ffcb0, L6:0x300278 (`hooks.glue`) |
| 22 | Rilgar's race organiser (class 918): a race that ends in its state 3 restarts the level's music, as its states 1 and 2 already do (the call `music_track_request(0, 8)`, L5:0x28f5c8, they make); and when the race earned the skill point (its flag 0x13d411), after the prize's message ("You won a Platinum Zoomerator!") the skill point's sound and "You just earned a Skill Point!" are given (the message takes the prize's place on the screen) | Rilgar's L5:0x316ab8 (`hooks.glue`) |

What the 1.02 build changed but is not ported: the memory card's code and screens (the 1.02 build rewrote them for its
own card library), the Japanese text layouts, the pad's reads (the host's input replaces them), the PAL disc 2.00's
movie code, the credits (two more lines), and changes that show nothing (recompiled code, a development timing display
removed). The row is read where each fix runs; it is changed on the main menu only, so a playthrough sees it from its
start. `RAC1_FIXES102=<n>,<n>...` (`all`, `none`) turns single fixes on for testing ([Environment variables](env-vars.md)).

## 1.02 Gameplay

The main menu's Advanced page has a third row of the later build's fixes, **1.02 Gameplay: Off / On** (off by default), a
global setting kept in the preferences only (`gameplay_102 0|1` in `advanced_options.txt`), never with a save: the 1.02
build's fixes that change how the game plays, most of them closing a way round the game (an exploit), which stays
possible with the row off. It is built as [1.02 Fixes](#102-fixes): each fix is the 1.02 build's own change, read from
its code (the PAL disc 2.00's, its addresses taken back through `data/regions/SCES-50916-2.00.txt`; the Japanese disc has
the same code), written into the North American port at the same place behind its own test (`gameplay102_on(n)`,
`src/game/enhance/fixes102.{h,cpp}`; a `(1.02 Gameplay, fix n: ...)` note at each site). The numbers are the analysis's
(F4, F8...), the ones `RAC1_GAMEPLAY102` takes. Where the change is in a hand-written port (the
weapons' updates: `ryno.cpp`, `visibomb.cpp`, `visibomb_ball.cpp`, `decoy_obj.cpp`), the test is in its code; where it is
in code the port transliterates from the disc (the levels' own copies of the hero's functions and moby classes), the
generators put a call there: `tools/codegen/hooks.glue` (each level's instruction, found by aligning the
level's copy with Veldin's and checked against the 1.02 build's copy of that level). With the row off each of those is a
test that fails, and the game's memory is the same frame by frame (RAC1_RAMHASH).

| Fix | What the 1.02 build changed | The way round it closes | Where (North American) |
|---|---|---|---|
| 4 | The RYNO's lock-on looks for its targets in the target list 0x1aba00 (the mobys of flag 0x1000 that moby_update queues each frame) instead of the whole moby list 0x15ffe4; and a target whose line from the shot is cut by a moby of type 5 (the targets' type) or 0xf is taken all the same | Probably: locking onto mobys the game does not queue as targets that frame (the original walks every moby), and a target hidden behind another one | `ryno_2cc360` 0x2cc360 (its 1.02 version in `fixes102_ryno_targets`, `ryno_asm.cpp` calling it) |
| 8 | The Visibomb's steering (the missile and its camera) ends in the hero state 0x32 too, the state a vendor or a character's talk puts Ratchet in | Probably: steering a Visibomb on while Ratchet is at a vendor or in a talk | `visibomb_update` 0x2b3058, `visibomb_ball_update` 0x2b6140 |
| 9 | Deadly ground (collision kinds 8 and 0xc, +0x12ea; the original takes Ratchet off it to the states 0x52 / 0x79, lost after 300 frames) now also for Ratchet in the Hologuise: its stand 0x53 and walk 0x54 go to its own 0x58 (lost after 300 frames) on it, as entering them does on the ground, and its 0x59 within 0.3 of it, 0x54 within 0.2; the hoverboard's ready stance (0x3f) rides off (2) only on its ground contact (+0x300), out of deep water (+0x12e7) and tilted less than pi/4 (+0x90 / +0x94), the original's height test gone; quicksand (+0x12eb) in a vehicle mode (+0x20a4 1 or 2) makes Ratchet lost (`hero_lost`) instead of sinking him | Probably: walking in the Hologuise over deadly ground (out-of-bounds floor) without being lost, riding the hoverboard off from over deep water, sinking a vehicle in quicksand | `hero_input_update` 0x229b70 (states 0x3f, 0x53, 0x54, 0x59), `hero_set_state` 0x2223f8 (0x53, 0x54), `hero_surface_update` 0x20b960; the same in Novalis's build (the hand ports `*_l1.cpp`, `hero_l2_small.cpp`: levels 1, 2, 3) and in the levels' own copies (5, 6, 8, 11, 12, 14, 15, 16, 17, 18: `hooks.glue`; the quicksand only in Rilgar's, Quartu's and Veldin's finale's, the others have none) |
| 10 | The characters that take Ratchet's input: a prompted one (the triangle's "talk" message: Pokitaru's class at 0x313290, Gemlik's 69, Drek's fleet's 1379) is not offered while Ratchet is already in the state 0x32 or 0x1d or in a vehicle mode, and its press puts him in 0x32 at once (the 1.02 build moved that call from the character's next step); Rilgar's 918 and Kalebo III's 1455 are not talked to (`talk_update`) while he is in 0x1d, and they lock his input (+0x20ac) as their scene starts; the card's screen (the flag 1 of 0x15eeb4) waits while he is in 0x1d | Probably: starting a talk from inside another talk, a vendor or a vehicle; a frame of free input between the press and the scene | `gameplay_frame` 0x297f78; `hooks.glue` at L11:0x313290, L13:0x2bb068, L17:0x2ed018, L5:0x316ab8, L16:0x2e6808 |
| 12 | Ratchet's tilt to the ground where he follows it (the ground mode +0x20b3, sub_00233440: on the hoverboard, states 0x3f / 0x70 / 0x71; in deep water with 0x13d4dc set; on the ground +0x308 marks): align_to_up's spring stiffer (0.8 of the frame time 0x15ed64 instead of 0.3, the damping kept at 0.3) and its speed capped at 1.0046 x 0x15ed6c (the original's cap 0: none) | Not understood: probably a tilt that runs away on the hoverboard over steep or broken ground | `sub_002163f0` 0x2163f0 |
| 13 | The Hologuise (item kind 0x1f): put on beyond the sub-states 0..2 in the hero states 4, 9 and 0xe (+0x2084) instead of the sub-state 4 (+0x208c); hero_place stops its countdown (+0x22de: putting it on or off 18 frames after the press); and a state forced on Ratchet in the Hologuise (hero_set_state, which takes him out of it first, `hero_mode3_exit`) stays his state | Not understood in full: the disguise put on or off across a teleport or a forced state | `sub_0020fe30` 0x20fe30, `hero_place` 0x216f90, `hero_set_state` 0x2223f8 (and its copies) |
| 14 | The Decoy Glove's decoy: while Ratchet swims at the surface (state 0x31) within 1.5 of it, the decoy gets a push from his place (the event 0x10000, strength 1000, that the game's bumps use), so it moves off instead of staying in his way | Probably: a decoy that blocks Ratchet in the water | `decoy_object_update` 0x2c4380 (`decoy_obj.json` `host_ntsc`) |
| 21 | Rilgar's water that rises and falls (class 831): it carries Ratchet swimming in it up as it rises (his water height +0x2f0 set to its surface while it is more than 0.1 from its target and he swims, +0x208c 0x11 / 0x12), and it holds still during a cutscene (game mode 2) | Probably: Ratchet left under the surface of rising water | Rilgar's L5:0x30ca80 (`hooks.glue`) |
| 23 | Giant Clank's pad (Quartu's class 1451, Veldin's finale's 1899): the press that turns Ratchet into Giant Clank first retires a moby still linked to the pad (its pvar +0x70, when alive) and, on Quartu, saves the pad's place as the checkpoint (`checkpoint_save` 0x284e50, the point 0.1 over the ground under the pad) before Ratchet is put there | Probably: Giant Clank's fight on Quartu restarted from the checkpoint before the pad, and a leftover moby of an earlier boarding | `clank_pad_update` (`shared_clank_pad.cpp`, the shared port of L15:0x2ed068 / L18:0x2fb868; the levels still run their generated copies until their re-glue) |
| 42 | The open sea of Pokitaru and Kalebo III (class 1111): each frame it is drawn (the camera above its height) it makes itself the level's flat water disc (0x1612dc: on, centre (512, 512), radius 1024, at the height of its swell), which `surface_height` (0x259e58) gives the hero's movement (his water height +0x2f0 and his entering the water +0x2f8), the bombs and the mines where no water region covers the place; the original leaves the disc off, so the sea's surface counts there as the ground's height. The same 1.02 change also sets the underwater colours (0x161200) while the camera is above 180: left out (below) | Not understood in full: probably Ratchet, the bombs and the mines over the open sea taking the sea floor's height for the water's | `class1111_update` (`shared_class1111.cpp`, the shared port of L11:0x30b358 / L16:0x2dee10; the levels still run their generated copies until their re-glue) |
| 66 | Gemlik Base's blob thrower (class 1885; Gaspar carries the same code): killed while Ratchet is at the height 250 (within 4) on Gemlik Base (the level number 13), its bolts get the flag 2 (`moby_drop_bolts`' flag 0x200), the one the game also gives them when Ratchet is next to the moby (they probably come to him at once) instead of being dropped where it stands | Probably: bolts lost where the throwers stand on Gemlik Base's platforms at that height, out of Ratchet's reach | `class1885_hit` (`shared_class1885.cpp`, the shared port of L9:0x30a9e8 / L13:0x30d3b8; the levels still run their generated copies until their re-glue) |

The 1.02 build changed more in the same places that is not ported here: in Novalis's and Veldin's crouch under a low
ceiling (0x81) Ratchet stands up after 20 frames without the Thruster-Pack on his back; Rilgar's class 918 has calls of
its own (the music, the skill point's message: [1.02 Fixes](#102-fixes), fix 22); the card's 0x400 flag and the
save block's checks in `gameplay_frame` are the 1.02 build's memory card. On the PAL disc 2.00 as the game's disc the
changed functions already are the 1.02 build's (its stand-ins), so the row adds nothing there except in the hand ports
(the shared hero code, `gameplay_frame`). The row is read where each fix runs; it is changed on the main menu only.
`RAC1_GAMEPLAY102=<n>,<n>...` (`all`, `none`) turns single fixes on for testing ([Environment variables](env-vars.md)).

### The later build's other changes to the level objects

The 1.02 build changed some forty functions of the levels' own code. Fixes 18 to 22, 10, 23, 42 and 66 above are the ones ported.
The others, read in the North American and the 1.02 build's code (PAL disc 2.00, the Japanese disc the same):

- No change to the game: Hoven's class 1404, Aridia's two skid classes, Eudora's 1190, Oltanis's 924 and Veldin's
  finale's 1422 follow the 1.02 build's new cutscene block (the hero's place flag at +0x4a, a byte); Blarg's 0x302400
  (its talking robots' lines) is recompiled; the Tesla Claw (0x2b94d0) is recompiled (its "six more calls" are the next
  function); the text numbers 5 higher are the PAL disc 2.00's own (its credits).
- Not on the PC: the Morph-o-Ray's chickens (0x2c97e0: start their culling countdown under load) and Gaspar's / Gemlik's
  class 1885 (no spawn past the first while the frame is loaded) shed load by the frame's load 0x15f5d0 / 0x15f5d4,
  which the port's timer (`runtime/ee/ee_timer.h`) keeps at 0: they would never run.
- Understood in the code, not in what it shows (left out): the Tesla Claw's hits carry an upward direction (+0x2b8 1.0
  instead of 0); Pokitaru's and Kalebo III's class 1111 and Pokitaru's 1158 set the underwater colours (0x161200..) when
  0x1677c8 passes 180; Umbris's class 1106 and its helper 0x313af0 test the bits 2 / 4 of a step's result instead of its
  value; Blarg's class 1051 keeps its 10-frame start once begun; Batalia's 467 removes its group (classes 0x24c..0x256,
  0x1d8) when its level flag 0x13d3b6 is set and its link is gone; Gemlik's class 170 (2.7 to 2.8), 1262 (a range of
  15), 0x2b4958 (a hit's +0x30 0.9 to 0.7) and 0x30bfb0 (+0x34 flag 0x20); Oltanis's 685 (only when Ratchet stands the
  same way up as it, a dot product over 0); Drek's fleet's 1448 and
  Veldin's finale's 1381 (a counter against a random wait); Pokitaru's 0x2d14b0 / 0x312838 and Gemlik's class 63 (large
  rewrites).

## Bug Fixes

The main menu's Options has a page **Bug Fixes**, right after Advanced, for small fixes of the original game's own bugs:
the ones no later build of the game fixed (the 1.02 build's own fixes are [1.02 Fixes](#102-fixes)), each a row of its
own, **Off / On**, off by default. With every row off the original's code runs and the game's memory is the same frame by
frame (RAC1_RAMHASH). Like the Advanced page's global rows they are settings of the whole game, kept in the preferences
only (one `bugfix_<name> 0|1` line each in `advanced_options.txt`), never with a save, and changed on the main menu only
(the pause menu has no Bug Fixes page), so a playthrough sees them from its start. The help box under the rows says what
the selected one changes.

This is where a future fix of the same kind goes: a bug of the original's code, with a small, local change that a player
may want, kept off by default so the original stays as it was. Each one gets a number (`RAC1_BUG_FIXES`), a row on the
page and a section in the table below; its change sits where the original goes wrong, behind its own test
(`bug_fix_on(n)`, `src/game/enhance/bug_fixes.{h,cpp}`, with an `enhancement: Bug Fixes, fix n` note at the site). A
change that alters how the game plays (an exploit closed, a difficulty changed) is not a bug fix in this sense: it
belongs with the gameplay rows.

| Fix | Row | What it changes | Where |
|---|---|---|---|
| 1 | Race Music After Quitting | Rilgar's hoverboard race: quitting from its results screen (game mode 4, kind 0, Cross) brings the planet's music back. The original never switches back there: the race organiser (class 918) restarts the level's music only for the race's other endings (its states 1 and 2, the call `music_track_request(0, 8)`; the 1.02 build added it to its state 3 too, [1.02 Fixes](#102-fixes) fix 22), so after a quit the race's track (track 2) kept playing on the planet (issue #132). The fix makes the same call at the quit: the level's music routine L5:0x28f5c8 with the track 0 (the level's music) as the next track, faded over through the track 8, as those endings do. Kalebo III's hoverbike race keeps the planet's music throughout and needs none | `screen_update` 0x202aa0 (kind 0, the quit's step 2), `bug_fixes_race_quit` |
| 2 | Hydrodisplacer Faucets | The Hydrodisplacer's faucets (class 0x155) keep Ratchet (issue #131). In the faucet's states 0x38..0x3a the hero is pulled onto the faucet's origin (the ride: 0x217970 block 0x21969c, and Rilgar's and Hoven's own builds of it); coming from the side away from zero, the position, rounded toward zero, lands exactly on that origin (from the other side it stops one float step short). Where an edge of the faucet's collision passes through its origin (Rilgar's first faucet, Hoven's first) the hero's ground probe then misses both triangles: he drops 0.06 onto the floor and leaves the ride, and the Hydrodisplacer's hand (0x2df9a0) waits for him on a faucet, so the water stays and circle does nothing until he steps off and on again or draws the item again (oracle batch 330: the original code does the same on the same memory). The fix, before the hero's update, puts him one float step short of the origin on the side toward zero, on each axis where he is on it or within 0.01 of it on the other side, while he rides a faucet: the pull stops there, as it does coming from that side | `gameplay_frame` 0x297f78, before `hero_frame_update`: `bug_fixes_faucet_ride` |

The page is built as the Mods page: a copy of the Camera page with its settings widget in the tall box and a copy of the
Options help label (its EE block 0xf1000..0xf2000, the Advanced block's free second half; its text ids 0x7b00..,
German, Spanish and Italian translated, French in English). It is not there with every enhancement off (the headless
default, the oracle). `RAC1_BUG_FIXES=<n>,<n>...` (`all`, `none`) turns single fixes on for testing
([Environment variables](env-vars.md)).

## Limits

- **Not taken from UYA:** its quick-turn triggers, idle braking, crouch values and crouch high jump,
  its strafe facing constants, the lock-strafe while firing, and its strafe flip's quadrant mapping
  (the flip uses R&C1's own quadrants).
- When backing into a wall while aiming, the camera can come very close.
- The page's French texts are shown in English.
- Buy All Ammo: the entry's hologram, the monitor's picture and the ammo counter at the top left show
  one weapon of the list (the one not full with the cheapest round), and after a purchase the counter
  can keep showing the weapon whose ammo was added last until the cursor moves.
