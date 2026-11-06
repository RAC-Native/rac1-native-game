# Mods

The game can load mods: folders of files that stand in for the disc's own, and textures that replace the game's by
their content. A mod brings its own work only: nothing of the game is ever shipped, and the game's own files are never
changed (turning a mod off gives the original back at the next start). This is the first step (phase 1): see
[What is not supported yet](#what-is-not-supported-yet).

## The mods folder

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
| `author`, `version`, `description` | shown under the list for the mod selected |
| `priority` | a whole number, 0 by default: when two mods give the same file, the higher priority wins |

The game's font has the plain ASCII letters only: other characters show as `?`. A folder without a readable `mod.json`
is listed as "Not loaded" and never used.

### Load order and conflicts

The mods on are taken from the highest priority down; between equal priorities, by folder name in alphabetical order.
When two of them give the same disc file or the same texture, the first in that order wins, and the game's log says so
(`[mods] conflict: ...`).

## Turning mods on and off

Main menu > Options > **Mods** (the entry is there when the mods folder holds at least one mod): one row per mod, On /
Off with cross or left / right; the box under the list shows the mod's description, author and version. A mod found for
the first time is on. A change applies at the next start (the row says "On at restart" / "Off at restart" until then).

The choices are kept in `mods.txt` in the settings' folder (`~/.config/rac1-native/` on Linux, `%APPDATA%\rac1-native\`
on Windows), one `<folder> on|off` line per mod.

While any mod is on, the title screen, the main menu and the pause menu say so at the bottom ("Mods on: 2"), so that a screenshot or a
report shows the game is not the original.

`RAC1_MODS=0` starts the game with every mod off, whatever the menu says.

## Disc files

The setup cuts your disc image into files (`assets/disc/`, see [Setup](setup.md#the-discs-files)), and the game reads
those files, at the sectors the disc's tables give. A mod's `disc/` folder is laid out the same way: a file there whose
path is one of the disc's (`assets/disc/layout.txt` lists them: `levels/level03/data.wad`, `toc/row_1234.vag`, ...) is
read instead of the disc's.

A mod's disc file must have **exactly the size of the original**: the game finds everything by its sector on the disc,
and a file of another size would move what follows. One of another size is not used (the log says so) and the disc's own
is read. Paths are case sensitive. The files are per disc: a file made from the North American disc (`SCUS-97199`)
fits that disc only, the European one has other sizes (and so a file made for it is not used on the other).

Reading from the image (`RAC1_READ_ISO=1`, a testing switch) and the data discs (the Japanese voices) take no mod files.

## Textures

A mod's `textures/<hash>.png` replaces the texture whose content has that hash, at any size (a larger PNG is a higher
resolution texture). It is used by the modern renderer (the MRP, the default):

- the level's textures: the mobys (characters, enemies, objects, the armors), the terrain (tfrag), the instances (tie)
  and the foliage (shrub), read at the level's load;
- the sky's textures;
- the 2D: the HUD, the menus, the fonts, the effects the 2D draws (the textures of the frame as the game samples them).

The faithful renderer (a debug choice, `RAC1_MRP=0`) draws the original textures.

The hash is computed from the texture as the game decodes it: FNV-1a 64-bit over 32-bit words, the width, the height,
then every pixel as RGBA (R in the low byte, alpha the GS's 0..128), written as 16 lower-case hexadecimal digits. Two
textures with the same pixels share a hash, wherever they are (the same texture on two levels is replaced on both).

The PNG's alpha is the usual 0..255, opaque at 255 (the game's 0..128 is scaled). Its mip levels (the smaller copies the
game draws far away) are made from it by the game. The game samples at most four levels (with Options > Graphics >
Texture Detail = Original, the GS's own choice among the original's): a much larger texture may shimmer far away.

### Making a texture pack: the dump

With the debug tools (`./build/rac1-native --debug-tools`, or the launcher's Enable debug tools), the debug menu's Game
tab has **Dump textures**: every texture drawn is written once as `<hash>.png` into `mods/_dump/textures/`, and a line
in its `index.txt` says what it is (`<hash> <kind> <width>x<height>`, the kind `moby`, `tfrag`, `tie`, `shrub`, `sky` or
`2d`). The level's textures are written at the next level load (go to another planet, or load a save), the 2D's as they
are drawn (those up to 256 x 256: the larger ones are mostly the frame's own pictures; they can still be replaced). A headless or debug run can dump with `RAC1_TEX_DUMP=1` (or `RAC1_TEX_DUMP=<folder>`).

To make a pack: dump, pick the textures to change, edit them (any size, keep the name), and put them in
`mods/<your-mod>/textures/` with a `mod.json`. The dumped PNGs are your disc's own textures: share your edits, not the
dump.

## Tests and the reference checks

A headless run (`RAC1_HEADLESS=1`: the tests, the reference images, the oracle) never looks at the mods folder: it plays
the original. `RAC1_MODS=1` loads the mods for a test (every mod found is on, the player's `mods.txt` not read unless
`RAC1_MODS_FILE` names one); `RAC1_MODS_DIR` gives another mods folder. With no mod on, the game is byte for byte the
original.

## What is not supported yet

Ideas for later phases, not done now:

- **Models and other assets of the modern renderer.** The MRP draws from data the setup generates from your disc
  (`assets/generated/`): a mod that changes a level's WAD changes what the game's code reads, but the MRP's models,
  their rig and the world's meshes stay the disc's. Replacing a model (keeping its skeleton) in the MRP is a later step.
- **Disc files of another size** (a larger level), which needs the game's tables rewritten.
- **Audio replacement** by file (music, voices, sounds as WAV or OGG), beyond swapping the disc's own `.vag` files.
- **A plugin API** for code (the host's hooks).
- **The launcher**: no Mods list there yet (the in-game menu and `mods.txt` are the way).
- **Mods while the game runs**: a change applies at the next start; textures are not reloaded during a level.
