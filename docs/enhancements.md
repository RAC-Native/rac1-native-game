# Enhancements

The port's first goal is fidelity: the game behaves as the original, proved function by function
(see [Overview](overview.md)). This page lists the improvements that sit **on top of** that faithful
port: those you can already turn on, and those planned for after the port is complete.

## The rules

- **Optional and off by default.** Anything that changes how the game plays or what its state is
  (controls, movement, the camera, pausing) is an enhancement. It is off unless you turn it on, and
  labelled as a change to the game. With every enhancement off, the game plays as the original.
- **Removing the PS2's compromises is a different thing.** Frame-rate drops, fades that hid disc
  loading, aggressive level of detail and draw distances, low texture resolution and the 4:3 frame
  were there because of the console, not the game's design. The port removes them where it can do so
  without changing the game's state, and keeps the original's way as an option.
- **The faithful path stays.** No enhancement replaces the original behaviour; it remains in the code
  as the reference.
- **Checked against the faithful path.** An enhancement that should not change the result (a faster
  renderer, a higher level of detail) is compared with the faithful path on the same frames: the
  game's state must stay identical. One that changes the result on purpose must leave the faithful
  result untouched when it is off.
- **After the faithful port.** An enhancement comes once the part of the game it touches is fully
  ported, not before.
- **Nothing from the disc is distributed.** Enhancements that need game data (such as HD textures)
  generate it on your machine from your own disc, like the setup does.

## Available now

### Gameplay: Options > Advanced

See [Advanced options](advanced.md).

- **Quick Select: Pause.** The world holds still while the quick select is open, as in the later
  games.
- **Movement: RAC 3.** Ratchet moves as in *Ratchet & Clank: Up Your Arsenal*: the run follows the
  stick's tilt, he turns and stops quicker, and L2 or R2 held makes him strafe.
- **Aiming: Modern.** A modern shooter's aim built on the strafe: L2 aims, R2 fires, the right stick
  turns the aim. It needs a strafe (Movement: Strafing or RAC 3); turning it on with Movement on
  Original sets Strafing.
- **Buy All Ammo: On.** The Vendor's list has an **All Ammo** entry first, as in *Up Your Arsenal*:
  one purchase fills every weapon's ammo, or as much as the bolts pay for, in the list's order. The
  entry is an item of the list itself, selected when the Vendor opens.

All four work on every level. The page also has **Fades** (Fast by default, Original as an option),
which is a removed compromise rather than an enhancement.

### Display and Graphics: Options > Display, Options > Graphics

See [Settings](settings.md#display) and [Rendering](rendering.md). The Display page holds the removed compromises (the first
items below); the Graphics page holds our additions behind one switch, **Graphics: Original / Enhanced** (Original, the
default, is the original's picture whatever the rows under it hold), with a preset (Low, Medium, High) for all of them: see
[Settings](settings.md#graphics).

- **Widescreen**: 16:9 or the window's own shape for the 3D, with black bars or stretched when the
  shape differs. The game itself still sees its own 4:3 view, so its state is unchanged.
- **A widescreen HUD** (the HUD row: 4:3 Original, 16:9, 21:9, 32:9, Fit Screen): in a widened view
  the HUD's side elements (ammo, the quick select, bolts, the air meter) move toward the sides of a
  box of that shape; health and the centred messages stay. Only where they are drawn changes. The
  menus, the pause screen and the Vendor stay 4:3.
- **Render resolution** from half to double the window's, and a **field of view** scale.
- **Level of detail** from Original to Maximum: objects keep their best model further away, without
  changing the game's own decisions.
- **Full textures at every distance** (the default): every mip level of every texture is kept, so
  distant objects stay sharp. The PS2's own mip choice remains available.
- **A frame-rate cap** for the picture (the game's logic always runs at 60 frames a second), and
  V-Sync.
- **The renderer**: [Vulkan](vulkan.md), the default, or OpenGL (from the next start), the same
  picture; OpenGL when Vulkan cannot start.
- **Ambient particles** (Graphics: Enhanced): our own dust, ash, embers, fireflies, light dust in the sunlight, rain and snow
  on some levels, purely visual; Batalia's rain and Hoven's snow drawn in place of the game's own while on. See
  [Ambient particles](particles.md).

### Modern lighting

See [Lighting](lighting.md). Off by default (Options > Graphics > Graphics: Original; Enhanced turns it on with every other addition).

- The sun's shadows, cast and received by the world and the objects (the levels' water, lava and
  sand seas too), with a Shadow Strength setting; the world lit by the sun, which a level's own file
  can turn (most levels have their own).
- The game's own dynamic lights (explosions, weapons) lit per pixel, and light emitters found
  automatically in the levels' glowing textures (lamps, panels, floor strips), with their shadows;
  a level's file can also make every object of a class a light (street lamps, neon tubes).
- Sun rays (Low, Medium, High): shafts of light from the sun where the sky shows through the scene;
  a level's file can turn them off there.
- Emissive surfaces: textures a level's file marks (lamps, screens, panels) keep the original
  lighting's brightness where the shadows would darken them.
- Ambient occlusion, in four modes, each at full or half resolution (SSAO, SSAO+, HBAO, GTAO; "1/2"), with an AO
  Intensity setting.
- Surface Detail (Off, Low, High): materials for the textures a level's file tags - shiny metal, wet
  ground with puddles and rain ripples, water, lava, mud - the PS2 textures kept, lit and animated;
  High adds reflections. See [Materials](materials.md).
- Grass (Off, Low, High): on the ground a level's file tags, a fine relief of grass blades (High: a few very low layers of
  blades). The textures' colours are kept. See [Materials](materials.md#grass).

### Sound: 5.1 and 7.1

Options > Sound's Mono / Stereo row also offers 5.1 and 7.1: the world's sounds come from their
direction, the dialogue from the centre. The game itself still plays in Stereo. See
[Settings](settings.md#sound-51-and-71).

### Movie subtitles

The original shows no text over its movies. Options > Language > Movie Subtitles (Off by default) shows subtitle files
over them, in the game's subtitle look. The setup can make them from your disc with speech to text, in the languages you
choose (`./setup.sh --movie-subtitles`, or the launcher's Settings); they are plain text files you can correct, and a
subtitles pack's files work the same way. See [Movie subtitles](movie-subtitles.md).

### New Game: Original or Modern, then the graphics

At New Game the game asks how to play, Original (the PS2's controls, cutscenes and Vendor) or Modern (the gameplay
enhancements of the Advanced page on), then which graphics to use: the Graphics page as it is (Current), Original, or our
lighting at the preset Low, Medium or High. The Display page is never changed there. Everything can be changed later. See
[Settings](settings.md#new-game-original-or-modern-then-the-graphics).

## Planned

In no fixed order. Each one is optional, and off by default if it changes the game.

### Picture

- **The rest of widescreen**: the level's baked shadow meshes, and the few surfaces the game queues
  only for the objects it drew in its 4:3 view, across the wider view (see
  [Rendering](rendering.md#limits)).
- **Higher frame rates.** The game runs its logic at 60 Hz, and many of its timers count frames. The
  safe route is to draw extra frames *between* two logic frames, interpolating positions, the camera
  and the animation poses, with the logic untouched.
- **HD textures**: for example upscaled textures, either generated on your machine from your own
  disc by a tool, or made by modders. The renderer would load a replacement by the original
  texture's fingerprint and fall back to the original.
- **Antialiasing and post-processing**: bloom, colour grading, depth of field, motion blur, each a
  separate toggle.
- **More modern lighting**: more lights placed by hand where the automatic ones fall short (most
  levels now have them, from Anya-Thoughtmane's lighting batches), labelled as our own art choices.

### Game

- **Cut content, optionally brought back**: things the game carries on the disc but never shows. Started: main menu >
  Options > Advanced > Cut Content brings back Kerwan's citizens and Eudora's Transport Ships, ported from the beta's
  code ([Advanced options](advanced.md#cut-content)).
- **A language switch** offering every language on the disc, without the console's system setting. Started:
  Options > Language sets the text, the voice and the subtitles apart ([Settings](settings.md#language)).
- **Quality of life**: a level select and similar conveniences for players.
- **Mods**: a mods folder in front of the disc's files the setup now extracts (`assets/disc/`, see
  [Setup](setup.md#the-discs-files)), and a Mods menu in the game (see [Roadmap](roadmap.md)).

### Platforms

- **macOS builds**: done, on Apple Silicon with a [Metal](metal.md) renderer (a contributor's port);
  other graphics back ends (Direct3D) may follow behind the renderer, beside OpenGL, the optional
  Vulkan one and Metal.
- **ARM64 and Android**: in progress on the `port/android` branch, not on `main` (#78); the CI has a
  Linux ARM64 job, not blocking yet.
- **Other releases of the game**: the European PAL discs 1.00 and 2.00 (the Platinum) run as the game's
  disc, and the Japanese disc gives its voices as a data disc; the Japanese disc as the game's disc, and
  the PAL 50 Hz mode, once the North American one is complete.
