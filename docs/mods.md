# Mods

The game can load mods: folders of files that stand in for the disc's own, and textures that replace the game's by
their content. A mod brings its own work only: nothing of the game is ever shipped, and the game's own files are never
changed (turning a mod off gives the original back at the next start). This is the first step (phase 1): see
[What is not supported yet](#what-is-not-supported-yet).

This page has five parts:

1. [Making a mod](#making-a-mod): a texture mod step by step, a sound swap, sharing a mod.
2. [Limits and gotchas](#limits-and-gotchas): what a mod can and cannot do today.
3. [Troubleshooting](#troubleshooting): every `[mods]` line of the log, and a checklist.
4. [Level mods](#level-mods): a new level the ship flies to (format 1), the example level, how it is built and saved.
5. [Reference](#reference): the folder, the manifest, the load order, the disc files, the textures, the dump.

The code: `runtime/kernel/mods.cpp` (the folder, the manifests, the disc files), `src/game/render/texture_mods.cpp`
(the textures and the dump), `src/game/enhance/mods_menu.cpp` (Options > Mods).

## Making a mod

### Before you start

- **The mods folder** is `mods/` in the folder the game is started from: the repository's root, next to `assets/`
  (the launcher starts the game from there). Make it if it is not there.
- **The log.** Everything the mods do is said in the game's log, in lines that start with `[mods]`. Started from a
  terminal, the log is the terminal's output; started from the launcher, it is the game folder's `build/game.log`.
  Read it after every change: it is the only place that says what was used and what was not.
- **A restart after every change.** The mods are looked at when the game starts. Nothing is reloaded while it runs.

### Make your first texture mod

The plan: dump the game's textures as PNG files, find the one you want, paint over it, put it in a mod folder.

#### 1. Start with the debug tools

The dump is a debug tool. Start the game with them: from the repository's root,

```
./build/rac1-native --debug-tools
```

or, in the launcher, turn on **Enable debug tools** and press Play. The debug menu opens with F1 (or R3 on a pad).

#### 2. Dump the textures

In the debug menu's **Game** tab, tick **Dump textures (for texture packs)**. The log says
`[mods] dumping the textures into mods/_dump/textures`.

Every texture the modern renderer draws is now written there once, as `<hash>.png`:

- the HUD, the menus and the fonts at once, as they are drawn;
- the level's own textures (the terrain, the scenery, the foliage, the objects and characters) **only at the next
  level load**: go to another planet, or load a save. The level you are on when you tick the box is not written.

To dump the first level without changing level, start the game with the dump already on:

```
RAC1_TEX_DUMP=1 ./build/rac1-native --debug-tools
```

`RAC1_TEX_DUMP=<folder>` writes into that folder instead (handy for one folder per level). Both need the debug tools:
without them the game removes its testing variables from its environment.

#### 3. Find the texture

`mods/_dump/textures/index.txt` has one line per texture written:

```
3f2a9c0d11e4b7a2 tie 64x64
```

the hash (the file's name), the kind and the size. The kinds:

| Kind | What |
|---|---|
| `tfrag` | the level's terrain |
| `tie` | the level's instanced scenery (rocks, buildings, machines) |
| `shrub` | the level's foliage and small scenery |
| `moby` | the objects and characters: Ratchet, Clank, enemies, crates, bolts, weapons, the armors |
| `sky` | the sky |
| `2d` | the HUD, the menus, the fonts, the screen effects |

The index does not say which level a texture is from, and a dump of a few levels holds thousands of files. To narrow
it down:

- **Dump one level at a time.** Empty the dump folder, start the game, dump, load the level you want, quit. A texture
  is written once per run and never again while the game runs, even if you delete the folder in the meantime: to
  start a fresh dump, quit first.
- **Find its kind and size in the debug menu.** The **Textures** tab shows the level's textures (tfrag, tie, shrub and
  moby) as thumbnails; hovering one gives its kind, its number in the level and its size (`tie texture 12 (64x64)`).
  With Graphics: Enhanced, the Lights tab's **Emissive surfaces and materials** section names the surface under the
  crosshair (or under a mouse click, with **Pick with the mouse**): `Crosshair: tie 7 tex 2 (the level's tie texture
  12)`, or `moby class 245` for an object. Type that texture or class number in the Textures tab's filter to see its
  textures. Neither tab shows the hash: they give you the kind and the size, and the index does the rest.
- **Search the index** for that kind and size, then look at those PNGs in an image viewer:

  ```
  grep ' tie 64x64' mods/_dump/textures/index.txt
  ```

The hash is computed from the texture's pixels. Two textures that look the same but differ in one pixel (or in their
palette) have different hashes, and each has to be replaced on its own.

#### 4. Edit it

Copy the PNG out of the dump and edit the copy. **Keep the file's name**: the name is the hash, and the hash is how
the game finds the texture.

- **Size.** Any size up to 8192 x 8192: a larger picture is a higher resolution texture, drawn over the same area as
  the original. Keep the original's proportions (a 64 x 32 texture as 256 x 128): another ratio is stretched to fit.
  Twice or four times the original is the useful range (see [Limits and gotchas](#limits-and-gotchas)). Sizes that
  are not powers of two are accepted.
- **Alpha.** Keep the alpha channel when the original has transparent parts (foliage, fences, grilles, the HUD's
  shapes). A PNG without an alpha channel is read as opaque everywhere: the holes become solid. In the PNG, alpha 255
  is opaque and 0 fully transparent.
- **The colour of transparent pixels.** The game makes the smaller copies seen far away (the mips) by averaging pixels,
  the transparent ones included. An editor that turns fully transparent pixels black gives dark fringes in the
  distance. Keep their colour when exporting (in GIMP: "Save color values from transparent pixels").
- **The format.** Any PNG the game's loader reads: RGB or RGBA, grey with or without alpha, indexed (a palette) with
  its transparency, 8 or 16 bits per channel (16 is reduced to 8), interlaced or not. Everything is turned into 8-bit
  RGBA. A file the loader cannot read is skipped with a line in the log (see [Troubleshooting](#troubleshooting)).

#### 5. Make the mod's folder

```
mods/
  my-first-mod/
    mod.json
    textures/
      3f2a9c0d11e4b7a2.png
```

`mod.json`:

```json
{
  "name": "My First Mod",
  "author": "Your name",
  "version": "1.0",
  "description": "A new look for the rocks."
}
```

Every field is optional (see [The manifest](#the-manifest)), but the file itself must be there and be valid JSON: a
folder without it is listed as "Not loaded". Subfolders of `textures/` are fine (`textures/veldin/...`): only the
file's name counts. Do not put your mod in `mods/_dump/`: folders whose name starts with `_` or `.` are not mods.

#### 6. Turn it on

A mod found for the first time is on. To check, or to turn it off: main menu > Options > **Mods** (the entry is
there once the mods folder holds a mod). A change applies at the next start.

#### 7. Restart and read the log

Quit and start the game again (the debug tools are not needed any more). At the start the log lists the mods:

```
[mods] my-first-mod: "My First Mod" 1.0 by Your name, priority 0, 0 disc files, 1 textures: on
```

At the first level load:

```
[mods] 1 replacement textures
```

and, the first time the texture is drawn:

```
[mods] texture 3f2a9c0d11e4b7a2 (tie 64x64) replaced: mods/my-first-mod/textures/3f2a9c0d11e4b7a2.png (256x256)
```

That last line is the proof: the original's kind and size, then your file and its size. If it is missing, see
[My texture is not replaced](#my-texture-is-not-replaced).

The replacement is drawn by the modern renderer (the default), with OpenGL and with Vulkan.

### Make a sound mod (a .vag swap)

A mod's `disc/` folder replaces the disc's files, and the disc's sounds are files too: the dialogue and many sounds
are `toc/row_XXXX.vag`, the levels' music and sounds `levels/levelNN/hdr_XXXX.vag`. A replacement must have **exactly
the original's size** (see [Disc files](#disc-files)). For these sounds that is practical, because the game plays a
VAG for the length its header gives, not the file's: a shorter sound, padded with zeros up to the original's size,
plays as a shorter sound. A longer one does not fit.

What you need: a PS-ADPCM (VAG) encoder. The project has none; the tools of PS2 sound modding make them (MFAudio is a
common one). A player that reads VAG files (vgmstream, for one) helps to listen to the originals.

#### 1. Find the file

Play until you hear the sound. Each sound the game streams leaves a line in the log:

```
[989snd] stream 04010002: VAGp at sector 0xc07, 18832 bytes, 44056 Hz (pitch 0xeaf), 32956 samples (0.75 s)
```

The sector times 0x800 is the file's first byte on the disc, the first column of `assets/disc/layout.txt`:
0xc07 x 0x800 = 0x603800, and

```
603800 18880 0 b1a6dcbd... toc/row_01a0.vag
```

is the file: 18880 bytes, of which 48 are the header and 18832 the sound, 44056 samples a second, 0.75 seconds.
Copy it out of `assets/disc/` to listen to it. The dialogue's file depends on the voice language (Options > Voice):
each language's lines are other files.

#### 2. Make the new sound

1. Make your sound **mono**, at the original's rate (44056 Hz here), no longer than the original (0.75 s here).
   A lower rate fits more seconds in the same bytes, at a lower quality: the header gives the rate, the game follows it.
2. Encode it as a mono VAG. The result is a 48-byte header (`VAGp`, then, big-endian, the sound's size in bytes at
   byte 12 and the rate at byte 16) and the sound, 16 bytes per 28 samples.
3. Check its size: it must not be larger than the original (18880 bytes). If it is, shorten the sound or lower the rate.
4. Pad it with zero bytes to exactly the original's size. On Linux or macOS: `truncate -s 18880 row_01a0.vag`. On
   Windows, a hex editor. The header's size stays your sound's size: that is what the game plays.

The loop and end flags inside the sound's blocks do not matter: the game sets its own.

#### 3. Put it in a mod

```
mods/
  my-voice-mod/
    mod.json
    disc/
      toc/row_01a0.vag
```

The path under `disc/` is the one `layout.txt` gives, letter for letter (paths are case sensitive). Restart; the log
says, at the start:

```
[mods] disc file toc/row_01a0.vag from my-voice-mod
```

A file of another size is refused with a line giving both sizes. A path that is not one of the disc's is ignored
without a word (the mod's line still counts it among its disc files): if neither line appears, check the path.

Limits: the file fits the disc it was made from only (the European disc's files have other sizes and sectors); the
Japanese voices come from the Japanese data disc, which takes no mod files; a looping sound (the music) loops at the
end of your sound.

### Sharing a mod

Ship the mod's folder, zipped, with only your own files in it:

```
my-mod.zip
  my-mod/
    mod.json
    textures/<hash>.png      the textures you changed, nothing else
    disc/...                 the disc files you changed, nothing else
```

The player unzips it into their `mods/` folder and restarts.

Never ship the dump (`mods/_dump/`), the textures you did not change, or any file copied from `assets/`: they are the
game's own data, from your disc.

Be plain about what an edit is:

- A texture **painted from scratch** at the right name is entirely yours. A texture made by **painting over the
  dumped one** still carries the game's art.
- A **modified disc file** (a level's `data.wad` with a few bytes changed) is the game's file with your changes: almost
  all of it is the game's data, and shipping it is shipping the game's data. A diff against the original (an xdelta
  or bsdiff patch) holds only your changes; the game cannot apply one, so the player makes the file from their own
  copy with the patch tool.
- A **.vag you encoded from your own recording** and padded holds nothing of the game: only its size matches.

What you share is your responsibility. The project ships no game data and takes none.

A texture pack is not tied to a disc: a texture whose pixels are the same on both discs has the same hash on both.
Textures that differ between discs (text in another language) do not match on the other. A disc-file mod fits the disc
it was made from only: say which one in the description (`SCUS-97199`, the North American; `SCES-50916`, the European).

## Limits and gotchas

What the code does today (`texture_mods.cpp`, `mods.cpp` and the renderers that call them).

**Textures**

- **The modern renderer only.** The faithful renderer (a debug choice, `RAC1_MRP=0`) draws the original textures.
  Both graphics APIs, OpenGL and Vulkan, draw the replacements.
- **Read once.** The mods' textures are listed at the first level load and each one is decoded the first time it is
  used; nothing is reloaded while the game runs. A texture changed or added while the game runs is seen at the next
  start.
- **When they are looked up.** The level's textures at the level's load, the sky's when it is first drawn, the 2D's
  when a picture is first drawn.
- **Same pixels, same texture.** A replacement applies wherever the original's pixels appear: on every level, and on a
  moby and a tie alike if they share the picture. One use of a shared texture cannot be replaced alone.
- **The 2D.** Pictures up to 1024 x 1024 can be replaced; those up to 256 x 256 are dumped. The larger ones (mostly
  the frame's own pictures) are not in the dump, so their hash is not known; those past 1024 x 1024 are never replaced.
- **The dump.** At most 20000 textures per run. A texture whose PNG is already in the folder is not written again, nor
  added to the index again.
- **Alpha.** The PS2's alpha goes from 0 to 128 (opaque); a PNG's 0..255 is scaled to it. The PS2 allows alpha above
  128 (blending "more than opaque"); the dump writes that as 255, and a replacement cannot go above opaque.
- **Size and memory.** Up to 8192 x 8192 a side. A replacement stays in memory, decoded with its mips (about 5.3 bytes
  a pixel: a 2048 x 2048 texture is about 22 MB), for the rest of the run once used, plus its copy on the GPU for each
  level that uses it.
- **Mips.** The game makes them itself (each a 2 x 2 average of the one above); a mod cannot give its own. The world
  and the mobys use four levels at most, the full size down to an eighth; the sky and the 2D use the full size only.
- **Far away.** With four levels, a texture much larger than the original has no small enough copy for the distance
  and may shimmer. With Options > Graphics > Texture Detail = **Original (PS2)**, the level drawn is the PS2's own
  choice, made for the original's size: a replacement four times larger is drawn with four times the detail at every
  distance, sharper near, more shimmer far. Two to four times the original is the useful range.
- **Proportions.** A replacement covers the original's area: another width-to-height ratio is stretched.
- **The modern lighting's materials and emissive lines** (a level's light file) are given by surface, not by picture:
  they stay on a replaced texture.

**Disc files**

- **Exactly the original's size**, or not used. Paths are case sensitive and must be one of `assets/disc/layout.txt`'s;
  another path is ignored without a word.
- **Per disc.** A file made from one disc fits that disc only.
- **Chosen at the start**, once.
- **Not everything reads them.** The data discs (the Japanese voices) and reading straight from the image
  (`RAC1_READ_ISO=1`, a testing switch) take no mod files.
- **A level's WAD is not its look.** The modern renderer draws from data the setup generated from your disc
  (`assets/generated/`): a changed `data.wad` changes what the game's code reads, but the models, the terrain and
  their textures stay the disc's. Change textures with `textures/`.

**Everything**

- **A headless run is the original.** The tests and the reference checks never look at the mods unless `RAC1_MODS=1`.
- **`RAC1_MODS=0`** turns every mod off, and the Mods page is not there in that run.
- **`RAC1_TEX_DUMP` and `RAC1_MODS_DIR` need the debug tools** (or a headless run): without them the game removes
  them from its environment. `RAC1_MODS` and `RAC1_MODS_FILE` always work.
- **The mods folder is relative** to the folder the game is started from.
- **The font** has the plain ASCII letters only: a mod's name, author or description in other characters shows `?`.

What no mod can do yet: see [What is not supported yet](#what-is-not-supported-yet).

## Troubleshooting

### The log's lines

Every line the mods write, in the order they come.

At the start:

| Line | Meaning |
|---|---|
| `[mods] RAC1_MODS=0: every mod off` | the variable is set: the mods folder is not looked at |
| `[mods] <folder>: no mod.json, not loaded` | the folder has no `mod.json` (the name is case sensitive) |
| `[mods] <folder>: mod.json is not a JSON object, not loaded` | `mod.json` is not valid JSON (a missing comma or quote, a trailing comma) or not an object `{ ... }` |
| `[mods] <folder>: "<name>" <version> by <author>, priority <n>, <d> disc files, <t> textures: on` | the mod is on in this run. `<d>`: the files under `disc/`; `<t>`: the `.png` files under `textures/`, whatever their names; `-` for a version or an author not given |
| `... : off` | the mod is off (Options > Mods, or `mods.txt`) |
| `... : not loaded` | its `mod.json` could not be read (the line before says why) |
| `[mods] disc file <path> from <folder>` | the disc file at `<path>` is read from that mod |
| `[mods] <path>: <folder>'s file is <x> bytes, the disc's <y>: not used (...)` | the mod's file has another size than the disc's (0: it could not be read): the disc's own is read |
| `[mods] conflict: <path> from <a> (priority <p>) over <b> (priority <q>)` | two mods give the same disc file: `<a>`'s is used |

At the first level load and while playing:

| Line | Meaning |
|---|---|
| `[mods] <n> replacement textures` | the textures the mods on give, counted by valid name (16 hexadecimal digits and `.png`). Fewer than the `<t>` of the mods' lines: some files are not named as a hash, and are ignored |
| `[mods] conflict: textures/<file> from <a> (priority <p>) over <b> (priority <q>)` | two mods have a texture at the same path: `<a>`'s is used |
| `[mods] texture <hash> twice: <file> kept, <file> ignored` | the same hash at two paths (two subfolders, of one mod or of two): the first in the load order is used |
| `[mods] texture <hash> (<kind> <w>x<h>) replaced: <file> (<w>x<h>)` | the texture was drawn and replaced; once per texture per run. The original's kind and size, then the file and its size (a 2D picture larger than 256 x 256 says `2d` too) |
| `[mods] <file>: not a PNG the game reads (<reason>)` | the file could not be decoded (the decoder's reason: a damaged file, not a PNG), or it is larger than 8192 pixels a side (`past 8192 pixels`): the original is drawn |

From Options > Mods and the dump:

| Line | Meaning |
|---|---|
| `[mods] <folder>: on at the next start` / `off at the next start` | a choice changed on the Mods page, saved in `mods.txt` |
| `[mods] cannot write <file>` | `mods.txt` or a dumped PNG could not be written (a read-only folder, a full disk) |
| `[mods] dumping the textures into <folder>` | Dump textures was ticked in the debug menu (not said for `RAC1_TEX_DUMP`) |
| `[mods] 20000 textures dumped: no more in this run` | the dump's limit: restart to dump more |

What leaves no line: a texture whose name is not a hash, a disc file whose path is not one of the disc's, a texture of
yours whose original is never drawn, and anything at all in a headless run without `RAC1_MODS=1`.

### My texture is not replaced

1. **Is the mod on?** Its line at the start must end in `: on`. No line for it: the folder is not in the `mods/` of
   the folder the game is started from, or its name starts with `_` or `.`.
2. **Is the file counted?** `[mods] <n> replacement textures` must count it. If the mod's `<t>` is larger than that
   count, a file is misnamed: exactly 16 hexadecimal digits then `.png` (`3f2a9c0d11e4b7a2.png`), not
   `3f2a9c0d11e4b7a2 (copy).png` or `3f2a9c0d11e4b7a2.png.png` (some systems hide the extension).
3. **Could it be read?** Look for `not a PNG the game reads`.
4. **Did another mod win?** Look for `conflict` and `twice` lines.
5. **Is it the right hash?** The texture you see must be the very picture you dumped. Dump again on the level where
   you see it and search the index for the hash. A copy of the texture on another level, or the same picture in
   another palette, has its own hash.
6. **Was it drawn?** The `replaced` line comes when the texture is first used. The level's textures are looked up at
   the level's load: after the restart, load the level.
7. **Which renderer?** The faithful renderer (`RAC1_MRP=0`, the debug menu's renderer choice) draws the originals.
8. **A large 2D picture?** Pictures past 1024 x 1024 are never replaced.
9. **A test run?** A headless run without `RAC1_MODS=1`, or `RAC1_MODS=0`, plays the original.

The line says `replaced` but it does not look right:

- holes became solid: the PNG has no alpha channel, or its alpha is 255 where it should be 0;
- dark or light fringes far away: the transparent pixels' colour was lost on export;
- shimmering far away: the replacement is much larger than the original, or Texture Detail is Original (PS2);
- squashed: the replacement's proportions are not the original's.

## Level mods

A level mod adds a **new level** to the game: a row on the ship's planet list, under the unlocked planets, that flies
there. The mod holds our own files only (a text file, OBJ meshes, PNG textures, VAG music); the setup **composes** them
with your own disc's files into the level's build, which never leaves your machine: a level of your own collision and
meshes, Ratchet, his ship and his gadgets on it, objects of the base level's classes (crates, its enemies, ...), a game
level's music or your own, a game level's sky, on the code of a **base level** of your choice (Veldin by default;
Kerwan, Eudora or Kalebo III: [Base levels](#base-levels)). Behaviour of its own (moving platforms, counters, HUD lines, rules
for its objects) comes from **code**: a plugin in the mod, built from its source by the setup (see
[Custom levels: code](custom-levels.md); a mod with code is off until you turn it on and confirm it).

The code: `src/setup/level_compose.cpp` (the composer), `src/host/level_slots.h` and `src/game/enhance/level_slots.cpp`
(the level's slot, its row, its saves), `src/formats/host_meshes.h` (the meshes the host draws),
`runtime/kernel/disc.cpp` (the level's files read as a disc's).

### Try the example level

```
python3 examples/custom-level/make_example.py mods/example-plateau
./build/rac1-native --setup --level-mods
./build/rac1-native
```

The first line writes the example mod (a floor, a ramp up to a platform, a few blocks coloured per vertex, a ship pad,
five crates, Kerwan's sky and "Overworld" by Daino Selvatico as its music (a stereo `.ogg`, CC BY 4.0:
`examples/custom-level/README.md`), made round (0, 0, 0) and moved by its offset; two classes of its own, sentries and
gems, with their models, seven sounds and a trigger; everything else in it is made by the script, the sounds too), and
copies its plugin (`examples/custom-level/plugin/`: a sixth crate that glides back and
forth as a platform, a HUD line, a sound where it turns, a visits counter in the save; the sentries' and gems'
behaviour, the trigger's chime). The second
composes it with your disc and builds the plugin (the setup does it too, as its last step: CMake and the compiler the
game was built with). In the game, the mod contains code: Options > Mods, turn **Example Plateau** on and confirm it
(cross twice), restart. Then enter your ship: the planet list ends with **Custom Levels / Example Plateau**. Its log
lines start with `[levels]`, `[compose]`, `[plugin]` and `[mod example-plateau]`. (A test run with `RAC1_MODS=1` but
not `RAC1_MOD_CODE=1` has the level without its code: the sixth crate sits still.)

### The level folder

```
mods/example-plateau/
  mod.json
  level/
    level.txt          the level's settings (below)
    collision.obj      what Ratchet stands on and bumps into
    meshes.obj         what is drawn (with meshes.mtl and its PNG textures)
    lights.txt         optional: the modern lighting's file for the level (lights, particles, materials, as
                       data/lights/levelNN.txt; the debug menu's Lights tab writes one)
    *.wav, *.ogg, *.vag   optional: the level's music and sounds (level.txt's music and sound lines)
```

`mod.json` says it is a level:

```json
{
  "name": "Example Plateau",
  "kind": "level",
  "level": { "id": "example-plateau", "format": 1, "base": 0 }
}
```

| Field | What |
|---|---|
| `kind` | `"level"` |
| `level.id` | the level's key: its progress in your saves and `RAC1_GOTO=<id>@<frame>` use it, never the folder's name. Letters, digits, `-`, `_`, `.` |
| `level.format` | `1` (the level runs on Veldin's code), `2` (on `level.base`'s) or `3` (a **session**: loaded once, a death brings Ratchet back to a checkpoint without loading the level again; Veldin's code by default, or `level.base`'s: [Sessions](#sessions-format-3)); a newer game may read newer ones |
| `level.base` | the game's level whose code the level runs: `0` (Veldin) in format 1; in formats 2 and 3 one of `0`, `3` (Kerwan), `4` (Eudora), `16` (Kalebo III): [Base levels](#base-levels) |

`level.txt`, one setting a line (`#` starts a comment):

| Line | What |
|---|---|
| `name <text>`, `name.<fr\|de\|es\|it> <text>` | the row's name (and the planet page's title), per text language |
| `region <text>`, `region.<lang> <text>` | the line above it |
| `hero auto`, `hero <x> <y> <z> [<yaw>]` | where Ratchet starts (and comes back after a death; in a session, format 3, the checkpoint `start` unless a `checkpoint start` line says otherwise). `auto` (the default when there is a ship): where he steps off the ship after the landing, beside it, facing away from it. A place of your own puts him there when the landing ends (as the game's planets do: the landing's last shot cuts to him there). Without a ship, a place is required |
| `ship <x> <y> <z> <yaw>` or `ship none` | where the ship stands and lands: the landing's camera and Ratchet stepping off follow its place and yaw, so keep about 20 units round it open (none: no landing, no way out but loading a save) |
| `death_height <z>` | below it Ratchet dies; default the base level's, or 8 under the collision's lowest point when the base level's is above it (the log says so) |
| `base <level\|name>` | the base level, as mod.json's `level.base` (a number, or `veldin`, `kerwan`, `eudora`, `kalebo`, ...): the composer checks they are the same (mod.json decides) |
| `spawn <class> <count>` | room for `<count>` objects of the base level's class `<class>` at once, for the plugin's `base_spawn` (ABI 3, [The mod SDK](sdk.md#base-level-objects)) |
| `import <name>`, `import <level>:<class>` | a class of another planet (a name of the game's `data/pool/names.txt`, as `umbris.turret`, or its level and class), in the level with what it spawns; the plugin spawns it (ABI 6, [The mod SDK](sdk.md#classes-of-other-levels-abi-6)). The setup's composer makes the asset pool from your disc the first time (a few seconds) and says in `compose.log` what each import brings. Classes that read their planet's paths or areas are given their own round where they are spawned, by their profile, which the plugin can change (ABI 8, [The mod SDK](sdk.md#the-level-data-of-imported-classes-abi-8)); drawn by the modern renderer only |
| `path <name> <x> <y> <z> [<x> <y> <z> ...]` | a path (1 to 64 points, moved by `offset` as every coordinate) for the plugin to give an imported object (`path_find`, `object_bind_path`: ABI 8), or a `moby` line's slot (format 3) |
| `session`, `checkpoint <name> <x> <y> <z> [<yaw>]`, `respawn reset\|keep` | format 3 only: [Sessions](#sessions-format-3) |
| `section <name> ... end`, `entry`, `portal`, `start`, `leave`, `moby <name>\|<level>:<class> ...` | format 3 only: [Sections](#sections); a section can be a game planet's world (`world`, `objects`, `box`: [Game worlds as sections](#game-worlds-as-sections)) |
| `hero <planet> [clank\|giantclank\|board]` | format 3 only, in main or a section: another planet's hero code and camera code run Ratchet there, as Clank, Giant Clank or on the hoverboard: [Hero modes](#hero-modes) |
| `boss <class\|name\|level:class>...` | format 3 only, in a section: its objects of those classes are its bosses (a death there starts the fight over; a boss beaten is not placed again, the plugin is told): [Bosses](#bosses) |
| `hero generic` | any format, on a base with hero code of its own (Kalebo III, Quartu, Rilgar): Ratchet runs on the generic hero code (Veldin's, Gaspar's, Umbris's): the game's own lava ([Hero modes](#hero-modes)) |
| `update_all` | every moby of the level is updated each frame wherever the camera is (the game updates a moby only within its update distance of the camera, moby +0x30: 64 units for Kalebo III's robots, a crate's less): enemies far away keep moving and fighting. Costs the updates of the mobys out of sight; the original's levels never |
| `background <r> <g> <b>`, `fog <r> <g> <b>` | colours (0..255) |
| `collision <file>`, `meshes <file>` | other file names (default `collision.obj`, `meshes.obj`) |
| `collision_type <n>` | the surface type of the collision's faces (default 31, plain ground) |
| `up z` or `up y` | the OBJ files' up axis: `z` (the game's, the default) or `y` (Blender's default OBJ export) |
| `offset <x> <y> <z>`, `offset auto` | added to every coordinate of the OBJ files (after `up` and `scale`) and of level.txt; `auto`: as much as the collision needs (below) |
| `scale <s>` | the OBJ files' units to the game's (default 1; Ratchet is about 1.5 units tall) |
| `moby <class> <x> <y> <z> [<yaw>]` | an object (below); format 3 also `[<slot>=<value> ...]`: its class's profile's slots ([Sessions](#sessions-format-3)) |
| `music <level>`, `music <loop> [<intro>]`, `music none` | the music: the game level's (0..18), your own (a WAV, an Ogg Vorbis file or a VAG, mono or stereo: below), or none; default the base level's |
| `sky <level>` | the sky of the game's level (0..18); default the base level's |
| `class <name> <model.obj> [size <s>] [solid\|ghost] [scale <k>]` | a class of the level's own objects (its model, its hit box; its behaviour in the mod's plugin): [The mod SDK](sdk.md#the-levels-own-content-leveltxt) |
| `object <class> <x> <y> <z> [<yaw>] [<key>=<value> ...]` | an object of one of the level's classes |
| `sound <name> <file.wav\|.ogg\|.vag> [volume <v>] [range <near> <far>] [loop] [pitch <s>]` | a sound of the level's own (the plugin plays it) |
| `trigger <name> <x0> <y0> <z0> <x1> <y1> <z1>` | a box Ratchet enters and leaves (the plugin's trigger callback) |
| `rail <name> [loop] [mesh [<material>]] [nostrip] <x> <y> <z> <x> <y> <z> ...`, `rails <file.obj> [...]` | grind rails (in main or a section block): [Grind rails and set pieces](#grind-rails-and-set-pieces) |
| `items <item> ...` | items Ratchet has while he is on the level (`grindboots`, `magneboots`, ...), never in a save: [Grind rails and set pieces](#grind-rails-and-set-pieces) |
| `class <name> <model.obj> ... collision <coll.obj>` | the class's collision a mesh of its own instead of its box: [Grind rails and set pieces](#grind-rails-and-set-pieces) |
| `picture <file.png>` | the planet on the loading screen and on the ship's map's planet page (below) |
| `banner <file.png>`, `banner.<fr\|de\|es\|it> <file.png>` | the name under it on the loading screen, per text language; default: made from `name` (and each `name.<lang>`) |
| `map_picture <file.png>` | the picture under the galaxy on the ship's map while the level's row is selected; default: made from `picture` |

**Pictures**: PNG files (any size: the composer scales them, 256 colours at most after it, as the game's are), with
alpha where nothing is to show:

| Line | The game's size | Shown |
|---|---|---|
| `picture` | 128 x 128 | the loading screen's planet, growing in the flight's last part; the ship's map's planet page (as it is: no turning, no map of it) |
| `banner` | 256 x 32 | under it, fading in (the game's own are blue letters about 12 pixels high, centred, on nothing) |
| `map_picture` | 306 x 108 | scaled to cover it, its middle kept |

With no `picture` the loading screen shows the base level's planet and the ship's map no picture (no planet). With no
`banner` the composer writes the name in the game's own font (its big menu font, read from your disc: Veldin's data
file's font texture and glyph table), in the game's blue; a name with letters that font has not (accented letters,
non-Latin ones) is written in a plain bitmap font instead (Latin letters; others become `?`).

**Objects** (`moby` lines): each is a copy of the first object of class `<class>` the base level places (its settings
and its private values), put at `<x> <y> <z>` turned `<yaw>` radians; it drops to the ground under it when its class
does. The class must be one the base level places: the composer says which when it is not. On Veldin these work: `500`
a bolt crate, `501` a nanotech crate, `511` an ammo crate; on Kalebo III its robots (`1445`): [Base levels](#base-levels).
Others may need what the base level's scripts give them (the other objects they point at, paths, missions, cutscenes)
and do nothing, vanish or misbehave: the composer copies none of that (their pointers to other objects are none, no
moby groups, no missions' spawn rules). Its spawn rule: an object the base level places for good once destroyed (a
crate) stays so; one the base level's missions place (most enemies) is gone for the visit once killed and back the
next time; either way it drops its bolts when killed. Each object gets an id
of our own, 1, 2, ... in the lines' order (the hero's skipped): what the game remembers of them in a save (a crate
broken stays broken) goes by it, so add new lines at the end to keep a save's.

**Music**: `music <level>` plays that game level's tracks (the base level's code picks the track: on Veldin, the
level's first one and its loop). Your own: `music <loop> [<intro>]`, `<loop>` looping (the whole file, end to start),
`<intro>` (optional) played once before it, each a **WAV**, an **Ogg Vorbis** file (`.ogg`) or a **VAG**:

- a **WAV** (PCM 8, 16, 24 or 32 bits or 32-bit float, mono or **stereo**, 8000 to 48000 Hz as it is, other rates
  resampled to the nearest of those): the composer encodes it, as the game wants it. Its length is made a whole number
  of ADPCM blocks (28 samples: resampled to that length, a stretch of at most 14 samples, no audible change) and its
  first block does not depend on what played before it, so the loop's end meets its start without a click: make the
  file loop well as it is (the end flowing into the start). A 44100 Hz stereo WAV stays 44100 Hz (the disc's music
  is 44100 Hz mono); about 50 KB a second of stereo.
- an **Ogg Vorbis** file (`.ogg`, mono or stereo, any rate as a WAV's): decoded when the level is composed (by
  [stb_vorbis](https://github.com/nothings/stb)), then encoded as a WAV is. The decoded length is the stream's own (its
  last granule position), so a loop that is seamless as a WAV stays seamless as an `.ogg` made from it: encode it as
  usual, for example `ffmpeg -i loop.wav -c:a libvorbis -q:a 6 loop.ogg` (about a tenth of the WAV's size). The
  example level's music is one.
- a **VAG** as the disc's music files (`levels/levelNN/hdr_014c.vag`): a 0x30-byte `VAGp` header (big endian: the
  data's size at +0xc, the rate at +0x10, 8000 to 48000 Hz), then mono PS-ADPCM, copied as it is. A **stereo VAG**
  (ours): byte +0x1e of the header 2, the size at +0xc one channel's, the left and the right channel's ADPCM
  interleaved in chunks of 0x800 bytes (the last chunk of each padded with zeros). The PS2 tools (MFAudio, ...) make
  mono ones.

**Stereo** plays on two of the sound chip's voices started together, the left channel's voice on the left and the
right one's on the right (as the game's own stereo, the movies' sound, is played), stepping through the file in sync;
the stream is otherwise one, as the game's music: its music volume (Options), pauses, fades and changes of track act on
both. A stereo intro with a mono loop (or the other way) is made stereo both (the mono one on both channels). An
intro and its loop play at one rate: a WAV or an `.ogg` is encoded at the other's rate (the loop's when neither is a
VAG); two VAGs of different rates are refused. In 5.1 / 7.1 the music is on the front left and right speakers, as the
game's.

The composer copies (or encodes) them into the build (they are read through the level's window); its log says what it
made of each (`music: ...`).

Coordinates are the game's: z up, in game units. **Every coordinate of the collision must be above 0** (the game's
collision grid starts at 0 and ignores what is below; the composer refuses a collision that reaches below). A level made
round (0, 0, 0), as a modelling program has it, is moved with `offset`: the example is made round (0, 0, 0) and has
`offset 200 200 20`; `offset auto` moves the level as far as its collision's lowest corner needs to be 16 units above 0
on each axis (in whole steps of 4; the composer's log says by how much). `lights.txt`'s coordinates are the game's
(the debug menu's Lights tab writes them so): they are not moved.

`collision.obj`: triangles or polygons (`v`, `f`), faces counter-clockwise seen from where they can be walked on (an
OBJ's usual front). `usemtl type<N>` before faces gives them surface type N. No collision of its own means no ground.
The surface types the base level's hero code knows act on Ratchet as on the game's levels: `type3` is **lava** (he sinks
into it and is bounced out, three times in a row and he is lost), `type1` the deadly liquid (he jumps out of it while he has health, else he is lost), `type31`
plain ground; Kalebo III's own hero code knows neither: on a level mod's level it gets them ([Base levels](#base-levels)).
`type2` is **magnetic**: with the Magneboots (owned, or level.txt's `items magneboots`) Ratchet walks up it, up walls and
onto ceilings ([Grind rails and set pieces](#grind-rails-and-set-pieces)).
The ambient particles' `source lava` line rises from a level mod's `type1` and `type3` faces ([Particles](particles.md#from-the-lava)).

**Materials on the meshes**: `lights.txt`'s `material` and `emissive` lines ([Materials](materials.md)) work on the level's own
meshes as on the game's ties: `material tie <n> tex <t> <type> ...`, `<n>` the mesh's class - the objects (`o`) of
`meshes.obj` in the file's order from 0, then the models of level.txt's `class` lines in their order -, `<t>` its
texture - the materials (`usemtl`) of that object in the order they first appear, from 0. For example `material tie 0
tex 0 lava 1.5 0.2` makes the first object's first material lava (its glow, its flow and its embers), `emissive tie 4
tex 1 2` the fifth object's second material glow. The debug menu's Lights tab names them so under the crosshair; the
composer's `compose.log` lists the meshes in that order with their textures' count (`mesh "lava": ... 1 textures`). Tested: the night arena's metals, lamps and its flame
class's model change with their lines (and not without).

`meshes.obj`: each object (`o`) is a mesh the host draws; each material (`usemtl`, in the `.mtl`) one texture: its
`map_Kd` PNG (scaled to powers of two, at most 256 x 256, at most 256 colours), else its `Kd` colour. At most 16
materials a mesh. A triangle's texture repeats at most 8 times across it (split larger triangles). **Vertex colours**:
`v <x> <y> <z> <r> <g> <b>` (0 to 1, as Blender exports them with colour attributes) colour the texture there (1 1 1:
as it is); a textured material's `Kd` tints it too. At most 64 colours a mesh (the game's instance colour table): more
are made coarser to fit (the log says so).

### How it is built: the composer

`./build/rac1-native --setup --level-mods` composes every level mod whose build is missing or old (its files changed, or
a new game version's composer); `--level-mod <folder>` composes one whatever its build; the setup runs the first as its
last step. The build goes to `assets/generated/mods/<folder>/` (made from your disc: never share it):

- `data.wad`: the level's data file. The base level's code, sound bank, HUD and VRAM image; a new asset index; new asset
  data: no terrain (tfrags), instances (ties), foliage (shrubs) or occlusion of the base level; its sky, moby classes,
  hero animations, gadgets and effects; the collision made from your `collision.obj`.
- `gameplay.bin`: the base level's settings, lights, generic cameras and help texts; Ratchet at your start; the ship (and
  its take-off path) or none; your objects (`moby` lines).
- `music_loop.vag`, `music_intro.vag`: your music, when level.txt names it (as the game plays it: a WAV or an `.ogg`
  encoded).
- `host_meshes.mrpc`: your meshes, drawn by the modern renderer (not by the game's code: the faithful renderer, a debug
  choice, does not draw them), and your classes' models.
- `level.sdk`, `sounds.bnk`: your classes, placed objects, sounds and triggers ([The mod SDK](sdk.md)); your sounds as
  one sound bank of the game's format.
- `mrp/world/level<base>/`: the modern renderer's world of the composed level (the meshes added).
- `picture.img`, `banner[.<lang>].img`, `map.img`: the pictures (level.txt's `picture`, `banner`, `map_picture`, or
  made by the composer), in the layout of the game's own pictures.
- `build.json` (the stamp), `compose.log` (what the composer did).

### How the game runs it: the level's slot

The level is a level of its own to you and to the host: **host slot** 19 (the game's levels are 0 to 18), running on its
base level's code (Veldin's, or `level.base`'s). Its files are read through a window of sectors of their own (no file of the disc is
replaced or padded), its lights file and world are its own, and the game's level number stays the base level's, so the
game's code is untouched.

- **The arrival** is a planet's: the loading screen's flight (its sound, the ship and its trails, the last segment with
  the planet's picture and name), then the ship's landing at level.txt's `ship` and Ratchet stepping off (`hero auto`),
  as on any planet. The game's special cases of Veldin as the first level (no landing, the short flight "to Veldin")
  are not used for the level, even on Veldin's code. With `ship none` there is no landing: Ratchet starts at
  `hero`.
- **The base level's progress is left alone**: the level never counts as a visit of its base planet (its visits, its
  time played, its visited flag), never unlocks it nor adds its gadget, and the base level's story cards and movies are
  left out on the way there and back ([Base levels](#base-levels)).

- **The ship's list**: one row per level mod on, in load order, after the unlocked planets (each its own host slot: 19,
  20, ..., 16 at most; the list's 20 rows permitting: with every planet unlocked there is room for two, the log says
  which levels have no row). Choosing one flies there; from it the ship flies anywhere as usual, to another level mod's
  level too. While a row is selected the galaxy page shows the level's `map_picture`, and its planet page the level's
  name (without the game's "Planet" before it) and region and its `picture`, nothing of the base level's (no missions,
  no gold bolts, no "Play Infobot": circle does nothing there).
- **Its state** (what Ratchet picked up and broke there) is its own: the save's per-level records of the base level hold
  the level's while you are on it, the base planet's are kept aside and come back when you leave.
- **Saves**: a memory card save made on the level is the game's own save, byte for byte (Veldin's records, the level
  number the game wrote). The level's progress goes in our `save<N>.pc` beside it, as an item of its own (its id,
  whether the save was made there, the level you flew from, its records). Loading that save with the mod on brings you
  back to the level. The Save and Load pages show the slot's planet from that item: the level's name, region and picture
  (its `map_picture`), or, with its mod off or missing, the level you flew from (where loading it lands). The level's
  plugin can save there itself, with no menu: the SDK's `game_save` ([Saving](sdk.md#saving-abi-15)).
- **The mod off or missing**: loading a save made on the level puts you on the level you flew there from, and opens the
  ship's planet map there: the **planet prompt**. Its list is the save's unlocked planets with the level you flew from
  first (and selected), then the other level mods on. Choose where to land, as from the ship; leaving the map
  (triangle or start) lands you on the level you flew from (it is loaded again: Ratchet is not in his ship there). Its
  progress is kept: saving again writes its item back as it was, so turning the mod on later finds it.
- **Options > Mods**: the level mod is a row as any mod. "Needs setup" when its build is missing or old (the box under
  the list says why); on or off at the next start.

### Level mods: limits

Format 1 is the first step:

- objects of the game: the base level's classes only, as copies of its own placements (see Objects above); no
  scripted events, no objects of other levels' classes. Objects of **your own classes** (`class` / `object` lines: a
  model, a hit box, behaviour in your plugin) and sounds of your own (`sound` lines) are the SDK's: see
  [The mod SDK](sdk.md);
- the sky: a game level's (`sky`), drawn by that level's own sky code (its stars, its turning shells) whatever the base
  level; a sky of your own is a later step (a texture mod can change a sky's textures in the modern renderer). Another
  level's sky is right in the modern renderer; the faithful renderer may show it with wrong
  colours;
- the base level: Veldin, Kerwan, Eudora or Kalebo III, each with its caveats: [Base levels](#base-levels);
- the level's size: the composed asset data (the base level's classes, sky and effects, and your collision) must not be
  larger than the base level's own (what the game's memory holds there: on Veldin 16 MB, of which the kept parts take
  about 9), the objects not more than the base level places, and the gameplay data not much larger than the base
  level's; the composer checks it (`[compose] budget: ...`) and refuses a level that is too large. A level of a few
  hundred units across is far within it;
- the collision is made of triangles: no quads (they would only make it smaller) and no hero collision groups (Veldin
  has none);
- the meshes are drawn by the modern renderer only (the faithful renderer, a debug choice, draws what the game's code
  sends: the meshes have no EE data), lit by the modern lighting;
- the North American disc (`SCUS-97199`); the PAL disc is not tested.

### Sessions (format 3)

A level of `"format": 3` is a **session** (the open engine's: [docs/open-engine.md](open-engine.md#sessions-stage-5)). It is
built and flown to as any level mod, and then:

- **It is loaded once**, at the arrival: until you leave it (the ship, a save of another level loaded) nothing loads it
  again.
- **A death is a soft reset**: Ratchet is back at the current checkpoint with his health full and the camera behind him; his
  weapons, ammo, bolts and items stay, and so do the plugin's state and the objects it started (its `respawn` callback
  decides what comes back: [The mod SDK](sdk.md#sessions-abi-9)). The level's placed objects (`moby` lines) killed since
  the arrival come back as placed (`respawn reset`, the default), except what the level's records keep (a crate broken
  stays broken); `respawn keep` leaves them as they are. On formats 1 and 2 a death loads the level again, as before.
- **Checkpoints**: `checkpoint <name> <x> <y> <z> [<yaw>]` lines (the level's `offset` applies); `start` is where the
  session starts Ratchet (without one, the `hero` line's place); the plugin chooses the current one (`checkpoint_set`,
  `checkpoint_at`).
- **Saves**: a save made in a session is the game's own save, as on any level mod's level; `save<N>.pc` also keeps the
  session's checkpoint and the plugin's session bytes, and loading the save brings you back there.
- **`moby` lines' slots**: `moby 1445 40 40 0.5 3.9 given_up=0` sets a slot of the class's profile (the game's
  `data/pool/profiles`) at the arrival: a `path` or `area` slot takes a `path` line's name, the others a number or `none`.
  The composer checks the slots against the profile (and every format's `import` lines of classes that read their planet's
  level data without a profile) and warns in `compose.log`.
- **Room for spawned objects**: as many as the base level's own placed objects leave free (`[compose] session: room for
  545 spawned objects` on Veldin).

```json
"level": { "id": "my-arena", "format": 3, "base": 0 }
```

```
session
checkpoint start 0 -60 0.5 1.571
checkpoint arena 0 -20 0.5 1.571
respawn reset
```

Log lines: `[session] the session "<id>": N checkpoints, respawn reset; no level load until it is left`, `[session] respawn N
at checkpoint "<name>" (...): no level load; placed objects reset: N back, N killed for good`, `[session] the session "<id>"
left (N respawns)`.

#### Sections

A session can have **sections**: parts with their own collision, meshes, sky, lights, music, fog, background, death height
and objects, which Ratchet goes between through **portals** (boxes he steps into) with no loading and nothing reset: his
weapons, ammo, bolts and health, the plugin's state and the objects it started stay. The lines outside any section block
are the section `main` (where the ship lands); each `section` block is another, its lines moved by the level's `offset` and
its own:

```
entry from_yard 20 -4 0.5 3.1416               # an entry of main (where a portal puts Ratchet, facing yaw)
portal east_gate 26 -4 0 31 4 6 to yard:west   # main's box: into it, Ratchet is in the yard at its entry west
leave remove                                   # main's objects when Ratchet leaves it: remove (default), freeze or keep

section yard remove respawn reset              # remove / freeze / keep its objects when it is left; its respawn policy
  offset 400 0 0                               # added to the level's offset
  collision collision.obj                      # its own (none: main's)
  meshes meshes.obj                            # drawn only while Ratchet is in it
  sky 16                                       # a game level's sky (none: main's)
  music 16                                     # a game level's music, or none (no line: main's)
  fog 120 60 40                                # background, death_height, lights <file>: as the level's lines
  entry west -20 4 0.5 0
  checkpoint yard_mid 0 0 0.5 0
  portal back -31 -4 0 -26 4 6 to main:from_yard fade 15   # fade: the screen darkened and back over 15 frames
  moby 500 4 -6 0.5                            # objects of the base level's classes, and of other planets':
  moby kalebo.robot 10 14 0.5 3.14             # a name or <level>:<class> places that class (it is imported)
end
start yard:west                                # optional: the session starts there once the landing is over
```

- **Its objects** are placed when the section is first in (frozen until Ratchet enters it). When he leaves it they are
  removed (the ones killed stay so when he comes back; the others come back as placed), frozen or kept running. A death in a
  section brings him back at the checkpoint there (the entry he came in by, or a `checkpoint_set`), and that section's
  killed objects back (its `respawn` policy, else the level's).
- **Saves**: a save in a section brings you back to that section at its checkpoint.
- **Only the current section is drawn**; sections may be anywhere (far apart is simplest: each has its own collision).
- The plugin goes between sections and streams them with the SDK's ABI 10 ([The mod SDK](sdk.md#sections-abi-10)).
- Log lines: `[compose] section <name> (...)`, `[section] <name> streamed in: ...`, `[section] frame N: main -> yard at entry
  west (portal east_gate): the switch 0.17 ms ...`.
- Limits: at most 31 sections; the level's own classes' objects (`object` lines) and triggers belong to the whole level; a
  section's music is a game level's or none.

#### Game worlds as sections

(The open engine's stage 8: [docs/open-engine.md](open-engine.md#game-worlds-as-sections-stage-8).) A section can be a **game
planet's world**: its scenery (drawn as on its planet, by the modern renderer), its collision, sky, music, fog and background,
its lights and death height, its level data (its cameras, paths, volumes and grind rails) and the planet's own objects you
choose (its enemies, crates, platforms), running on its own code:

```
section kalebo remove respawn reset
  world kalebo                     # (or a number: 16) the planet's world: everything below its lines can change
  objects kalebo.robot 52 638 500  # its placed objects of these classes come with it (a name, its class id, or `all`)
  box 130 110 110 200 275 140      # (optional) only its objects inside this box (x0 y0 z0 x1 y1 z1)
  entry arrive 146.43 133.23 128.3 1.46     # the planet's own coordinates (no offset in a world's section)
  checkpoint kalebo_start 146.43 133.23 128.3 1.46
  portal back 140 136 125 144 140 132 to main:from_kalebo fade 10
  sky 3                            # optional: another sky, music, fog, background, death_height, collision instead of its own
  zones all                        # optional: every zone of its world drawn wherever the camera is (default: zones grid)
end
```

- **Coordinates** in a world's section are its planet's (the level's `offset` does not move them, and the section has no
  `offset`): put entries where Ratchet stands on that planet (where its level starts him: Kerwan 263.98 102.09 54.5 facing
  1.5708, Kalebo III 146.43 133.23 128.3 facing 1.46). The section's camera is the planet's own (its first camera, as at its
  start).
- **Objects**: none come unless an `objects` line names them. Each chosen class is imported (the composer adds an import of
  it and of what it spawns; the base level's own classes, such as the crates 500..511, are the base's); the planet's spawn
  rules apply as on a first visit (objects that appear only after a story event there do not). `compose.log` lists them:
  `[compose] section kalebo: level 16's world (...); 59 of its 1568 objects in its box: 52 x5, 500 x35 (the base's), 638 x8,
  1445 x11, uids 10..68`. The game keeps at most 224 classes at once (the base level's and every import of the session):
  `objects all` in a large box can reach it (objects of classes of code alone, such as cameras and triggers, are not placed:
  the log says so). An object resting on another object needs that one chosen too: Kerwan's two crates by its start sit on
  a lift (class 1210); without `1210` they fall through when the section is entered (they are not broken, so a death does not
  bring them back).
- **What is drawn** of the planet's world is what the planet itself draws from the camera's place: its zone grid (cells of 4
  units, each with the list of the world's zones seen from there) leaves out what that cell does not see, and a camera off
  the grid (high above the planet, or out of its paths) keeps the last cell's list. A `zones all` line in the section draws
  every zone of the world wherever the camera is (the planet's tfrags, ties, shrubs and objects; a little more to draw):
  for a camera the planet never had, such as over Quartu's Giant Clank arena, which Ratchet on foot never sees from inside.
  `zones grid` is the default.
- **Memory**: a world takes about 18 to 20 MB of the session's memory above 64 MB (Kerwan 19.2 MB, Kalebo III 18.0 MB), read
  from your disc while you play, before you enter it (the log's `[section] kalebo: level 16's world: ...`).
- **Rails of your own** in a world's section: `rail` / `rails` lines, in the planet's coordinates (as its entries): the planet's
  own rails stay, yours are added (the SDK's `rail_find` and `hero_rail_attach` find them). A rail's collision strip goes into
  a copy of the planet's collision the composer makes for the section (the planet's faces with the strips: without a strip
  the grind drops Ratchet), so the section's `collision.bin` is about the size of the planet's. A rail's `mesh` model is
  drawn in the planet's world while the section is shown (GL and Vulkan). Its model is a host mesh like the others, but it
  comes **after all of them, the `class` lines' models included**: a world section's rail model shifts no `material tie
  <n>` index of lights.txt (the order: `meshes.obj`'s objects, main's rail models, the other sections' meshes and rail
  models, the `class` models, then the world sections' rail models, section by section). `compose.log` says where they
  went: `[compose] section quartu: its rails' 1 models drawn in level 15's world, host meshes 7..7 (after the classes': no
  other index moves)`. Example (the arena's escape continued into Quartu's arena, Ratchet attached by the plugin in its
  `section_enter` callback):

  ```
  section quartu remove respawn reset
    world quartu
    entry arrivo 130.5 186 61 1.5708
    rail quartu_in 130.5 186 60 130.5 200 48 130.5 210 36 130.5 216 28
  end
  ```
- **The hero code** there is main's (`hero generic` in main holds in every section without a `hero` line of its own).
- **The base level's enemies** (a level on Kalebo III's code: its robots spawned with `base_spawn`, hunting with `base_hunt`)
  follow Ratchet into a world's section and fight there as anywhere in the session.
- **Not in a world's section**: `meshes` and `moby` lines of the base level's classes (its own scenery is the planet's;
  `moby <name>|<level>:<class>` lines are fine).
- **The level's own objects in a world's section**: `object` lines inside a section block (any section: a world's in its
  planet's coordinates, another's moved by its `offset`) place objects of the level's classes there; they are spawned at
  the level's start with main's. An object belongs to its section (its `object` line's), or to the section it was spawned in
  (`object_spawn`), or last moved or shown in (`object_place`, `object_show(obj, 1)`). While a world's section is current
  only its objects are there: drawn, and solid as their class is; every other object is neither drawn nor solid (main's,
  at main's coordinates, would stand somewhere in the planet's space). Outside world sections a world section's objects are
  neither drawn nor solid, and the rest is as before (drawn in every section). So the plugin brings main's ring to the top
  of the Quartu rail with `object_place(ring, top, rot, 0)` in its `section_enter` callback (or an `object ring ...` line in
  the `section quartu` block puts one there); back in main, an object brought into the world is there again once it is
  placed or shown in main. Tested (`rm-escape`): main's pillar hit by a ray in main and not in Quartu, Quartu's pillar the
  other way round, before and after a return to main.

#### Hero modes

(The open engine's stage 9: [docs/open-engine.md](open-engine.md#hero-contexts-stage-9).) In a session Ratchet runs on
Veldin's hero code (every move: rails, Magneboots, swimming, lava). A section (or main, outside every block) can have him run
on **another planet's own hero code** and that planet's own camera code instead, and be **Clank**, **Giant Clank** or on the
**hoverboard**, with the game's own code for each:

```
section race remove respawn reset
  world kalebo
  hero kalebo                      # Kalebo III's hero code: his own board riding, its race camera
  objects 439                      # Kalebo III's race board: he gets on it when he walks up to it
  entry board 145.3 90.3 83.7 -1.5708
end
section giant remove respawn reset
  world quartu
  hero quartu giantclank           # Quartu's hero code and Giant Clank's camera; Ratchet is Giant Clank in the section
  entry start 130.5 222.4 26 0
end
section clank remove respawn reset
  world orxon
  hero orxon clank                 # Clank (Orxon's), on Orxon's camera code
  entry start 229.65 203.13 49.3 0
end
```

- **The planets** with hero or camera code of their own: `kalebo` (16: the hoverboard, its race camera), `quartu` (15: Giant
  Clank's states and camera), `gemlik` (13: its camera type), `orxon` (10: its camera type), `rilgar` (5: its board, held to
  Rilgar's race track). Another planet is refused by the composer.
- **The modes**: `clank` (the composer imports Orxon's Clank, 10:87), `giantclank` (Quartu's Giant Clank, 15:419), `board`
  (the planet's board, 16:439 or 5:439; needs `hero kalebo` or `hero rilgar`). The mode's object is spawned where Ratchet comes
  in, and he becomes it once he stands there (after the switch's fade); leaving the section he is himself again at the next
  section's entry, his health as before (Clank's is his own while he is Clank). A board can also be one of a world's objects
  (`objects 439`): he gets on it by walking up to it, as on the planet. A death in a mode brings him back at the checkpoint
  in it again.
- **Not here**: the race's other racers and its laps' rules (the HUD shows the race: lap 1/3); Giant Clank's HUD picture
  in a section without `world quartu` (a world's section brings its planet's HUD pictures: [Bosses](#bosses)). Off a world's
  planet the Kalebo III board rides anywhere (its jumps, its falls: back where he got on).
- **Gemlik's ship** (`world gemlik` with `objects 69`, `hero gemlik`): it waits at its pad, Ratchet boards it with triangle
  and flies it as on Gemlik Base; a crash brings him back at the section's checkpoint with the ship at its pad. Its flight
  ends at once when nothing is left to shoot at: `objects 69 388` brings Qwark's ship, which flies its route. Qwark's fight
  itself is not done ([docs/open-engine.md](open-engine.md#bosses-stage-10)).
- **`hero generic`** (main's line, **any format**: also a format 2 level on Kalebo III's base): Ratchet runs on the generic hero
  code, the one Veldin, Gaspar and Umbris run, instead of the base's own (Kalebo III's): the game's own **lava** (`type1`
  faces, Gaspar's and Umbris's: he jumps out scorched (0x3c), a health point lost each time, and sinks (0x7c) with none
  left) and its **sinking ground** (`type3` faces: he sinks in it (0x68) and Cross bounces him out (0x69); left in it he is
  lost, the level loaded again in formats 1 and 2 as a fall; no damage of its own), with their animations (from Veldin's
  core, read from your disc at the level's load) and their sounds (from Veldin's bank, below), and every generic move. Everything else stays the base's: its objects and enemies on its own code, its cameras, rails ridden as
  before. Only the base's own riding goes (Kalebo III's hoverboard). Kalebo III's lava stand-in (the hop out, below) is then
  not used. `hero generic` with the start's numbers is still Ratchet's start (`hero 0 -67 0.5 1.571`).
- The plugin does the same with the SDK's ABI 12 ([The mod SDK](sdk.md#hero-modes-abi-12)): `hero_build`, `hero_mode`
  (`hero_build(RAC1_HERO_BUILD_GENERIC)` on any format).
- **Magneboots on a base other than Veldin**: a base level whose hero has no magnetic-walk animation (Kalebo III's) gets it
  from Veldin's (read from your disc the first time Ratchet steps on a magnetic face, put in his class): he no longer looks
  wrong on Magneboots walkways there (any format).
- Log lines: `[compose] section race: Ratchet runs on level 16's hero code and camera code`, `[hero] the hero context: level
  16's hero code and camera code run Ratchet (...)`, `[hero] mode 2 entered (section giant): ...`, `[hero] the hero's sequence
  91 (empty in his class) from level 0's core: ...`.

#### Bosses

(The open engine's stage 10: [docs/open-engine.md](open-engine.md#bosses-stage-10).) A boss fights in its planet's world as a
section, on its own code: the **Snagglebeast** in Umbris's cave:

```
section cave remove respawn reset
  world umbris
  objects all                      # the cave's objects: the boss, its pillars, the platforms, the crates
  box 195 355 25 320 500 90
  boss umbris.snagglebeast         # (or 1106, or 7:1106) the section's boss
  entry pit 263.35 483.16 36.5 -1.13
  checkpoint cave_start 263.35 483.16 36.5 -1.13
end
```

- **What comes with a world's section** (any world's, boss or not): its planet's **camera code** (its own camera types and
  camera hooks, as on the planet), its planet's **HUD pictures** the session lacks
  (the boss's health bar with its portrait; Giant Clank's energy bar in `world quartu`), and its planet's **level state** as
  on a first visit: the records its objects read and write (what was collected, what was killed for good) are the session's
  own copy for that planet, never the planet's in your game (beating the Snagglebeast in a level mod does not beat it on
  Umbris, and the planet's progress does not hide the boss from the level mod).
- **`boss <class|name|level:class>...`**: the section's objects of those classes are its bosses (the section must place them:
  `objects` or a `moby` line). **Ratchet's death in the section** starts the fight over: Ratchet back at the checkpoint (the
  boss's own checkpoints count: the Snagglebeast's at its death), the boss placed again as at the start (its health, its
  phase). **A boss beaten** (it leaves play: the Snagglebeast's death sequence, about 7 seconds) is never placed again in this
  visit; the game does not end, nothing else happens by itself: your plugin is told (the SDK's `boss` callback, ABI 13) and
  does what follows (a portal, a reward, a fade back to main). A save made after it keeps it beaten.
- **Not yet**: a boss in a section of the mod's own geometry (the Snagglebeast waits for Ratchet inside its cave's box and
  follows its routes there: Umbris's coordinates); the other bosses (Qwark's space fight, Nebula G34's creature, Drek).
- Log lines: `[hero] the camera level: level 7's camera code runs the cameras (...)`, `[section] level 7's HUD: 1 animations
  the session lacks added (...)`, `[section] cave: its boss (class 1106) placed again after the death: ...`, `[section] frame
  2384: cave: its boss (class 1106, moby 01085c00) beaten (1 in this visit); the session goes on`.

### Grind rails and set pieces

(The open engine's stage 7: [docs/open-engine.md](open-engine.md#rails-and-set-pieces-stage-7).) Every format, in main or in a
section block:

```
items grindboots magneboots                       # his for the visit: given at the arrival, taken back when he leaves,
                                                  # never in a save (the card's save is written without them)
rail low mesh -24 -20 1.2 24 -20 1.2              # a rail from its points (two at least); `mesh`: its model made (a bar
                                                  # 0.2 wide whose top is the line, grey metal, or `mesh <material>` of
                                                  # meshes.obj's .mtl)
rail circuit loop 0 0 3 10 0 3 10 10 3 0 10 3     # `loop`: its end joins its start
rails rails.obj mesh pad                          # each object of the OBJ's line elements a rail named as the object
                                                  # (a curve converted to a mesh in Blender, its edges: `l`); a closed
                                                  # one is a loop
class deck deck.obj collision deck_col.obj        # the class's collision a mesh of its own (below)
```

- **Rails**: the points are moved by `offset` (and the section's) as every coordinate; the composer puts a point every 1.5
  units along them and writes them into the gameplay file's rail block (main's, or the section's own: the game's rail
  table is the current section's). Ratchet rides them with the game's own grind: the Grind Boots, landing on them, jumping
  along and across (to a rail 2 to 4.5 units beside: lanes), swinging the wrench, flying off the end. A rail rides along its
  points' order. A thin collision strip (0.5 wide, just under the line, `collision_type`'s surface) is added under each:
  the game's grind lets go of a rail with nothing under Ratchet's feet (`nostrip`: none, for a rail over your own
  collision). On a level mod with rails of its own, the landing works anywhere: Kalebo III's, Oltanis's and Blarg's hero code
  otherwise takes a rail only near its own level's rail spots. The plugin finds them by name (ABI 11: `rail_find`,
  `hero_rail`, `rail_point`, `rail_enable`, `hero_rail_attach`, the `rail` callback: [The mod SDK](sdk.md#rails-and-the-set-piece-kit-abi-11)).
- **Items for the visit**: `grindboots`, `magneboots`, `helipack`, `thrusterpack`, `hydropack`, `o2mask`, `pilotshelmet`,
  or an item id (its owned flag only: the held gadgets need the quick select). One he owns already is his own: neither
  given nor taken.
- **Magnetic surfaces**: `usemtl type2` faces in the collision. With the Magneboots, Ratchet walks onto them and stays on
  them up curves, walls and upside down on ceilings (the game's magnetic walk: the camera turns with him). Tested on
  Veldin's code (format 1, and sessions) and on Kalebo III's: Kalebo III's hero class lacks the magnetic walk's animation, so
  there it is taken from Veldin's (read from your disc the first time he steps on a magnetic face; before stage 9 his model
  was drawn wrong there).
- **Collision meshes**: `class <name> <model.obj> collision <coll.obj>`: its objects collide as that mesh (made round the
  model's origin, `up` and `scale` as the model's; 255 vertices at most), turned and scaled with the object, not its box: a
  deck with walls, an arch, a ring. The plugin moves one along a path with Ratchet riding it (`object_follow_path`), or
  where it likes (`object_place`).
- The plugin's set-piece services: a screen fade, a camera of its own (fixed, looking at, along a path, following), hazards
  (volumes that hurt on a timer): [The mod SDK](sdk.md#rails-and-the-set-piece-kit-abi-11).
- Log lines: `[compose] rail <name> (main): 33 points, 48.0 units, its model`, `[compose] rails: their collision strips, N
  triangles`, `[compose] class "deck": its collision mesh deck_col.obj, 36 triangles`; `[setpiece] level slot 19: 4 rails of
  its own (the hero code's rail spots not needed), 2 items for the visit`, `[setpiece] 2 items given for the visit`,
  `[setpiece] 2 items of the visit taken back` (`RAC1_SETPIECE_TRACE=1`: the rails' events, the hazards, the camera).

### Base levels

A level runs on its **base level**'s code: its overlay (its classes' behaviour, its hero's moves, its sky's code), its
moby classes, its sound bank, its hero animations, its gadgets and effects. The composer keeps all of these and drops the
base level's terrain, instances, foliage, collision and placed objects. Format 2's `level.base` chooses it, among the
bases tested (others are refused: "not a base level this game runs a level on"):

| Base | `level.base` | What it brings | Caveats |
|---|---|---|---|
| Veldin | 0 (format 1's) | the most hero animations; bolt, nanotech, ammo crates | no enemy that works alone tried; no gold bolt class (the pause menu's and the ship's map's "Found" panel shows a plain bolt instead on the level) |
| Kerwan | 3 | its classes | its hero code ends a fall below z 5 at once (the composer warns; `offset auto` keeps a level higher) |
| Eudora | 4 | its classes | none found |
| Kalebo III | 16 | its classes, its robots (enemies) | its hero code ends a fall below z 77 at once (also a fall from one of its own moves, the hero's state 0xf, with no air left): keep the level above 77 (`offset`); the composer warns. Its hero code has no lava and no deadly liquid: the level gets them (below) |

On every base the level is a planet of its own: the base planet's progress is never touched (its visits, its time,
its visited flag, its unlock on the ship's list, its gadget added to the quick select, its kill and pickup records:
the level has its own), and the base level's story cards and movies of a first flight there (Eudora's, Gemlik Base's,
Kalebo III's) or of leaving it are left out on the way to and from the level. The base level's sound table is the
SDK's `sound(index)` (its indices change with the base). The faithful renderer's look of another level's sky on a base
is not checked.

**Kalebo III's lava.** (Or the game's own lava with `hero generic`: [Hero modes](#hero-modes).) Kalebo III's own build of the hero code (its surface update L16:0x2097a0, its ground probe
L16:0x20ffb0) has no surface type 3 (lava) nor 1 (deadly liquid): on its own level those faces are plain ground. On a
level mod's level (never on the original's Kalebo III) the game adds the generic build's: the faces flagged as on the
other levels (the hero's +0x12e6, +0x12e5), the lava not a floor (its top kept, the ground probed under it), and, as
Ratchet comes down into either, a hop out of it while he has health; the third hop within 3 seconds (or no health) and
he is lost (the fade, the respawn at the level's start, as a fall). Not the same as the other levels': their lava's
states (0x68 sinking and bouncing on Cross, 0x7b / 0x7c / 0x7d sinking to death) are not in Kalebo's hero code, nor
are their animations (Kalebo's hero has 33 of Veldin's animations fewer, 0x71 the lava's among them); its 0x69 / 0x3c
(the generic lava's and deadly liquid's jumps out) hold him in place when entered without 0x68, so the hop is the
standing jump. Bringing the real ones would mean, on a level mod's Kalebo, the states' entries (`hero_set_state`), their
updates (`hero_state_update`), their exits (`hero_input_update`'s 0x68 / 0x7b tails: the Cross jump, `lost_timer`), each
a hook into Kalebo's generated code as above, and the animations (Veldin's hero animations, a different set, added to the
composed level's hero class). No damage from the lava itself (the other levels' does none either; a plugin can add it:
`hero_damage`). `RAC1_LEVEL_TRACE=1` prints the hops (`[levels] Kalebo III's hero: ...`).

**Kalebo III's classes**, each placed twice with `moby` lines on a flat arena and watched headless (Ratchet
standing, then swinging the wrench, `RAC1_SIM_INPUT=attack`):

| Class | Placed on Kalebo III | Seen |
|---|---|---|
| `1445` | 52 | **the robots**: walk to Ratchet, hit him, take the wrench's hits, die (blown up), drop their bolts. Placed and with `base_spawn` |
| `52` | 21 | **the bomb bots** (round, a red eye: they roll at Ratchet and blow up). Placed they watch an area of the base level's (a path): use `base_spawn`, which gives them their own (SDK ABI 5) |
| `541` | 29 | **the Gadgetron robots** (crash-test-dummy markings; a bomb launcher, a club or a flamethrower): on their level hidden behind the doors of a Gadgetron hall until a trigger (654) walks them out. Placed they stay hidden; with `base_spawn` (ABI 5) they come out at once and fight |
| `638` | 19 | **the jetpack troopers**: start on a path of the base level's or inside a dropship. Placed they move to (0, 0, 0); with `base_spawn` (ABI 5) they hover and fight where spawned |
| `1356` | 5 | **the dropships**: fly in along a path, drop their troopers, fly off. Placed they move to (0, 0, 0); with `base_spawn` (ABI 5) they come down to the point spawned at with up to 4 troopers |
| `470`, `471`, `546`, `552`, `654`, `758`, `805`, `1135`, `1455`, `270` | | stay where placed, no attack (props, triggers (654 wakes the Gadgetron robots), hazards that need the base level's links, a character (1455)) |
| `556`, `1410` | | move to (0, 0, 0) at once: they follow another object or a path of the base level's. Do not use |
| `1826` | | drops through the floor (a path of the base level's). Do not use |
| `1377` | | starts a cutscene of the base level's when Ratchet is near. Do not use |
| `1401` | | gone at the start. Do not use |

`spawn` lines and the SDK's `base_spawn` give the robots at run time (waves): [The mod SDK](sdk.md#base-level-objects); the
bomb bots, the Gadgetron robots, the jetpack troopers and the dropships too from ABI 5
([Kalebo III's enemies](sdk.md#kalebo-iiis-enemies-abi-5)).
Testing another base: `RAC1_DEBUG_TOOLS=1 RAC1_LEVEL_BASE_ANY=1` lets the setup compose and the game run a level on any
of the game's levels (every one composes; only the four above were run).

### Level mods: log lines

| Line | Means |
|---|---|
| `[mods] <folder>: the level "<id>" on host slot 19` | on in this run |
| `[mods] <folder>: the level "<id>" is not on: needs setup (...)` | no build, an old one, or the mod's files changed since: run the setup |
| `[mods] <folder>: the level "<id>" is not on: too many level mods on (16 at most)` | more level mods on than host slots |
| `[levels] the ship's planet list is full (20 rows): "<id>" and the levels after it have no row` | every planet unlocked: no room for more rows |
| `[mods] <folder>: level.format N ...: needs a newer game` | a format this game does not read |
| `[levels] "<name>" chosen` | its row chosen (or `RAC1_GOTO=<id>@<frame>`) |
| `[levels] level 0's header: level slot 19's` | the level loads |
| `[levels] level slot 19's music: ...` | the level's music: none, a game level's, or the mod's (its sectors in the window) |
| `[levels] the save was made on the level "<id>", whose mod is off or missing` | the planet prompt |
| `[compose] offset: (x y z) added to every coordinate` | level.txt's offset (or auto's) |
| `[compose] hero auto: Ratchet steps off the ship at (x y z)` | where he starts (`hero auto`, or no `hero` line with a ship) |
| `[compose] picture: ...`, `banner: ...`, `map picture: ...` | the pictures written (`picture.img`, `banner[.<lang>].img`, `map.img`) |
| `[levels] the loading screen: level slot 19's planet picture the mod's, its name the mod's` | the loading screen's pictures (or the base level's when the build has none) |
| `[compose] budget: asset data X MB of the base level's Y MB` | how much of the base level's room the level takes |
| `[compose] death height: the base level's ... is above the collision's lowest point ...` | no `death_height` line and the base level's would kill Ratchet on the level: one under it instead |
| `[compose] warning: the collision reaches z ..., below the base level's own kill height ...` | Kerwan's or Kalebo III's hero code's fixed kill height: keep the level above it |
| `[compose] spawn pool: class C, N objects (uids ...)` | a `spawn` line's room |
| `[sdk] level slot S: N objects in the spawn pools (base_spawn)` | taken out of play at the level's load, ready for `base_spawn` |
| `[mods] <folder>: level.base N: not a base level this game runs a level on (...)` | a base not in the allow-list |
| `[compose] ...` | the composer's steps; its errors name the file and line |

### Testing a level mod

`RAC1_MODS=1 RAC1_GOTO=<level.id>@60` (headless: also `RAC1_HEADLESS=1`) starts on Veldin and goes to the level at
frame 60. `RAC1_LEVEL_TRACE=1` prints the records' swaps, the `.pc` items and where the landing leaves Ratchet (`the landing's
end`, in the ship's frame); `RAC1_LEVEL_SAVE_IMAGE=<frame>:<prefix>`
writes the card's save blocks and our items at that frame (a save made on the level must give the same level blocks as
one made just before flying there). `RAC1_LEVEL_MAP_AT=<frame>` opens the ship's planet map at that frame (as a take-off
does), to choose a row with a `RAC1_SIM_INPUT=seq:` script; with `RAC1_LEVEL_TRACE=1` the list's rows and the row
selected are printed. With a level mod in `mods/` and its build present but no `RAC1_MODS=1`, a headless
run is the original, byte for byte.

## Reference

### The mods folder

`mods/` at the repository's root, next to `assets/` (the folder the game is started from). Each mod is a folder of its
own:

```
mods/
  my-mod/
    mod.json                    the manifest (below)
    disc/                       files laid out as the disc's extracted files (assets/disc/)
      levels/level03/data.wad
      toc/row_1234.vag
    textures/                   replacement textures, by hash
      3f2a9c0d11e4b7a2.png
```

Folders whose name starts with `.` or `_` are not mods (`mods/_dump/` is where the texture dump goes).

### The manifest

`mod.json`, a JSON object; every field is optional:

```json
{
  "name": "Blue Bolts",
  "author": "Someone",
  "version": "1.0",
  "description": "The bolts in blue.",
  "priority": 0
}
```

| Field | What |
|---|---|
| `name` | shown in the Mods menu (the folder's name without it; 40 characters at most) |
| `author`, `version`, `description` | shown under the list for the mod selected (cut at 40, 16 and 200 characters) |
| `priority` | a whole number, 0 by default: when two mods give the same file, the higher priority wins |

The game's font has the plain ASCII letters only: other characters show as `?`. A folder without a readable `mod.json`
is listed as "Not loaded" and never used.

### Load order and conflicts

The mods on are taken from the highest priority down; between equal priorities, by folder name in alphabetical order.
When two of them give the same disc file or the same texture, the first in that order wins, and the game's log says so
(`[mods] conflict: ...`).

### Turning mods on and off

Main menu > Options > **Mods** (the entry is there when the mods folder holds at least one mod): one row per mod, On /
Off with cross or left / right; the box under the list shows the mod's description, author and version. A mod found for
the first time is on. A change applies at the next start (the row says "On at restart" / "Off at restart" until then).

The choices are kept in `mods.txt` in the settings' folder (`~/.config/rac1-native/` on Linux, `%APPDATA%\rac1-native\`
on Windows), one `<folder> on|off` line per mod (`on-code` for a mod with code turned on and confirmed: a mod that gains
code after it was turned on asks again).

While any mod is on, the title screen, the main menu and the pause menu say so at the bottom ("Mods on: 2"), so that a
screenshot or a report shows the game is not the original.

`RAC1_MODS=0` starts the game with every mod off, whatever the menu says.

### Disc files

The setup cuts your disc image into files (`assets/disc/`, see [Setup](setup.md#the-discs-files)), and the game reads
those files, at the sectors the disc's tables give. A mod's `disc/` folder is laid out the same way: a file there whose
path is one of the disc's (`assets/disc/layout.txt` lists them: `levels/level03/data.wad`, `toc/row_1234.vag`, ...) is
read instead of the disc's.

A mod's disc file must have **exactly the size of the original**: the game finds everything by its sector on the disc,
and a file of another size would move what follows. One of another size is not used (the log says so) and the disc's own
is read. Paths are case sensitive. The files are per disc: a file made from the North American disc (`SCUS-97199`)
fits that disc only, the European one has other sizes (and so a file made for it is not used on the other).

Reading from the image (`RAC1_READ_ISO=1`, a testing switch) and the data discs (the Japanese voices) take no mod files.

### Textures

A mod's `textures/<hash>.png` replaces the texture whose content has that hash, at any size (a larger PNG is a higher
resolution texture). It is used by the modern renderer (the MRP, the default):

- the level's textures: the mobys (characters, enemies, objects, the armors), the terrain (tfrag), the instances (tie)
  and the foliage (shrub), read at the level's load;
- the sky's textures;
- the 2D: the HUD, the menus, the fonts, the effects the 2D draws (the textures of the frame as the game samples them).

The faithful renderer (a debug choice, `RAC1_MRP=0`) draws the original textures.

The hash is computed from the texture as the game decodes it: FNV-1a 64-bit over 32-bit words, the width, the height,
then every pixel as RGBA (R in the low byte, alpha the GS's 0..128), written as 16 lower-case hexadecimal digits (an
upper-case name is read too). Two textures with the same pixels share a hash, wherever they are (the same texture on
two levels is replaced on both).

The PNG's alpha is the usual 0..255, opaque at 255 (the game's 0..128 is scaled). Its mip levels (the smaller copies the
game draws far away) are made from it by the game. The game samples at most four levels (with Options > Graphics >
Texture Detail = Original, the GS's own choice among the original's): a much larger texture may shimmer far away.

#### Making a texture pack: the dump

With the debug tools (`./build/rac1-native --debug-tools`, or the launcher's Enable debug tools), the debug menu's Game
tab has **Dump textures**: every texture drawn is written once as `<hash>.png` into `mods/_dump/textures/`, and a line
in its `index.txt` says what it is (`<hash> <kind> <width>x<height>`, the kind `moby`, `tfrag`, `tie`, `shrub`, `sky` or
`2d`). The level's textures are written at the next level load (go to another planet, or load a save), the 2D's as they
are drawn (those up to 256 x 256: the larger ones are mostly the frame's own pictures; up to 1024 x 1024 they can still
be replaced). A headless or debug run can dump with `RAC1_TEX_DUMP=1` (or `RAC1_TEX_DUMP=<folder>`).

To make a pack: dump, pick the textures to change, edit them (any size, keep the name), and put them in
`mods/<your-mod>/textures/` with a `mod.json`. The dumped PNGs are your disc's own textures: share your edits, not the
dump. Step by step: [Make your first texture mod](#make-your-first-texture-mod).

### Tests and the reference checks

A headless run (`RAC1_HEADLESS=1`: the tests, the reference images, the oracle) never looks at the mods folder: it plays
the original. `RAC1_MODS=1` loads the mods for a test (every mod found is on, the player's `mods.txt` not read unless
`RAC1_MODS_FILE` names one); `RAC1_MODS_DIR` gives another mods folder. With no mod on, the game is byte for byte the
original.

### What is not supported yet

Ideas for later phases, not done now:

- **Models and other assets of the modern renderer.** The MRP draws from data the setup generates from your disc
  (`assets/generated/`): a mod that changes a level's WAD changes what the game's code reads, but the MRP's models,
  their rig and the world's meshes stay the disc's. Replacing a model (keeping its skeleton) in the MRP is a later step.
- **Disc files of another size** (a larger level), which needs the game's tables rewritten. (A new level of your own:
  [Level mods](#level-mods).)
- **Audio replacement** by file (music, voices, sounds as WAV or OGG), beyond swapping the disc's own `.vag` files.
- (Code in mods, a native plugin: done, see [Custom levels: code](custom-levels.md).)
- **The launcher**: no Mods list there yet (the in-game menu and `mods.txt` are the way).
- **Mods while the game runs**: a change applies at the next start; textures are not reloaded during a level.
- **A texture's own mips**, or a replacement for one use of a shared texture.
