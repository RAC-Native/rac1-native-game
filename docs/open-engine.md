# The open engine

Work in progress, for developers. Nothing here is visible to players yet.

The goal is custom levels that run on the game's own engine with far more freedom than a level mod has today: any class of
any planet in one level, sections brought in and out while the game keeps running (no level load, no reset of the hero,
the weapons or the level's state), and more memory than the PS2's 32 MB. A custom level of this kind is a **session**:
the game loads one level, the session's base (Veldin's engine), and the host brings everything else in and out. The
original game is untouched: none of this runs unless a session is on, and every change is checked against the reference
pictures and the RAM hashes of the original's levels.

The work is staged; each stage ships on its own. This page describes what is real now. Stage 1 (this one) laid the
foundations and ships no visible feature: more memory for sessions, a heap above 64 MB, the engine's data told apart from a
level's own, and the asset pool's index. Its proof is the tests below.

## Session memory

The original's EE memory is 64 MB: the PS2's 32 MB, and above them the host's own areas
(`runtime/level/host_layout.h`). A session grows it to 256 MB.

- `runtime/kernel/ee_memory.cpp` keeps the RAM in one reserved block (`platform::memory_reserve`: an anonymous mapping on
  POSIX, reserved address space on Windows) whose pages cost memory only once they are touched. The usable size starts at
  64 MB; `ee_session_memory(bytes)` grows it (a power of two up to `ee_session_ram_max()`, 256 MB by default; nothing
  wires 256 in, `RAC1_SESSION_RAM_MB` tries other sizes up to 512) and `ee_session_memory(0)` puts it back, dropping what
  was above 64 MB.
- What follows the size: the inline accessors' limits, the journal's page table, `ee_is_valid_addr`, and two values that
  stay the original's outside a session: `ee_game_ram_end()` (32 MB, or the session's RAM) and the uncached mirrors'
  fold, `ee_mirror_phys()` (a 0x2xxxxxxx / kseg address folds onto 32 MB as on the PS2, or onto the session's RAM).
- The host's sanity checks of the game's pointers that assumed 32 MB now ask `ee_game_ram_end()`: the modern renderer's
  class packets and vertex data (`mrp_moby_renderer.cpp`), its LOD records and moby walks (`mrp_lod.cpp`), the lighting
  tables, the particles' collision tree, the tfrag GPU path, the chain decoder's end (`mrp_scene.cpp`), the pipelined
  frame's copy of the moby array (`orig_main.cpp`, `ee_copy.cpp`), libgraph's transfers, the plugins' address check
  (`mod_code.cpp`), the movie subtitles' font.
- The DMA chain walkers read a tag's address through `ee_dma_addr_mask()` (32 MB, or the session's RAM): the faithful
  renderer's (`runtime/gs/gs_emu.cpp`), the modern renderer's moby and world chains (`mrp_chain.cpp`,
  `mrp_world_scene.cpp`). This one was found by the test of a class in high memory (below): with the 32 MB mask a class's
  packets above 64 MB were read from the wrong place.
- Left as they are: `RAC1_RAMHASH` and `RAC1_RAMDUMP` (the original's 32 MB checks), the oracle's comparisons, the game's
  stack at the top of the 32 MB.

`RAC1_SESSION_MEM=1` turns a session's memory on for a whole run (testing). The reference runs give the same pictures and
the same RAM hashes with it, and so does a level mod with code (Kalebo III's arena, 9000 frames of play with the wrench):
nothing the game does depends on the RAM's size.

## The session heap

`src/game/enhance/session.cpp`: blocks of 64-byte alignment from 0x04000000 to the session RAM's end, each tagged with its
owner (`pool 0:500` for a pool class's block; later a section, a level's context), freed one by one or by owner, coalesced.
Nothing of the game allocates there by itself: the host places everything. `session_heap_log()` prints its use,
`RAC1_SESSION_TRACE=1` every allocation, `session_heap_hash()` is a session's own check beside `RAC1_RAMHASH`.

## Engine data and a level's own data: the extents

Every level ships its own build of the engine, linked at other addresses, with the level's own code and data between the
engine's objects. The overlay maps (`tools/overlay_match/overlay_match.py`, `assets/generated/overlay_map/levelN.txt`) pair
the addresses Veldin's code names with the level's. For a cross-level class (stage 2) every address of a level must be
classed: the engine's (the session's copy is used) or the level's own (the class's own copy is used). The maps alone
cannot say where an engine object ends: an engine array far past its mapped base (the class id map, 0x1ec0 bytes; the
moby texture copy) looked like the level's own data.

`tools/overlay_match/extents.py` (run by the setup after the maps) writes `overlay_map/levelN.ext`: the map's data pairs
as blocks of one displacement, each with its extent past its last mapped address, up to the nearest of: the next mapped
address in Veldin and in the level, the section's end, the first address only the other build's own code or own data
names. Pairs that are values rather than addresses (a "page" constant both builds load, lui + a small offset) are left out.
The game reads the extents beside the maps (`src/game/ovl/ovl_reloc.cpp`):

- `ovl_xlate_data` moves an address inside a block with its block (without extents, another region's disc or an old
  setup, the old nearest-mapped-address rule stays);
- `ovl_data_to_veldin` tells a level's engine data (Veldin's address) from its own (0).

`RAC1_OVL_EXT_DIFF=1` logs every address where the extents and the old rule disagree: on all 19 levels they agree on every
address the ports use, so the original plays as before. The check of the classification, on a RAM dump:

```
RAC1_HEADLESS=1 RAC1_LEVEL=16 RAC1_FRAMES=392 RAC1_RAMHASH=/tmp/h.txt RAC1_RAMDUMP=390,/tmp/d16.bin ./build/rac1-native
python3 tools/overlay_match/extents.py --levels 16 --check /tmp/d16.bin
```

It counts the level's data words written by that frame that each rule calls "own", and checks that each run the extents
call own lies in an object only the level's own code or data names. At frame 390:

| Level | Own data written, old rule | Own data written, extents | Runs in an own object |
|---|---|---|---|
| 16 Kalebo III | 33.9 KB in 511 runs | 2.9 KB in 12 runs | all |
| 7 Umbris | 71.8 KB in 1163 runs | 46.5 KB in 732 runs (two 16 KB buffers of Umbris's own) | all |
| 6 Blarg | 29.8 KB in 580 runs | 3.3 KB in 142 runs | all |
| 13 Gemlik | 37.4 KB in 980 runs | 6.8 KB in 261 runs | all |

A level's own data is far smaller than the old rule said: 61 KB on Kalebo III (408 KB by the old rule), 109 to 226 KB on
the others.

## The asset pool

`src/game/enhance/pool.cpp` makes `assets/generated/pool/` from the disc's levels (any disc: it reads them through
`disc_read_level_wad`), only when something needs it: `rac1-native --setup --pool`, a session, a test. It is never in the
repository and never shared, like every generated file.

| File | What |
|---|---|
| `classes.bin` | every level's moby class blocks as the disc stores them (unrelocated: the game's own `moby_class_register` relocates a copy wherever it is put), each once by content, 64-byte aligned |
| `pool.idx` | the index, text: a `pool` line (version, disc), a `level` line per level (its counts and sizes), a `class` line per level and class: the block's place, size and hash, the class's update function and +0x2c word from the level's class table, its texture remap |
| `pool.json` | the stamp: the version and the disc; a pool made from another disc or by an older version is made again |

A class block runs to the next item of the level's asset data (the block and its padding). Two setups give the same bytes.
`rac1-native --setup --pool-report` prints the pool per level and in all; on the North American disc: 3641 class rows,
88.9 MB of blocks, 1350 class ids of which 291 are in more than one level and only one (class 2 on level 4) has other
bytes there; `classes.bin` is 1183 blocks, 41.7 MB.

`RAC1_POOL_CHECK=1` compares the pool with the game's own loads: each class the level setup registers against its row
(the block before relocation, the remap, then the update function and the word the registration gave). Every class of all
19 levels is equal.

Still to come in the pool (stages 2 to 4): sounds, the levels' contexts (their own data images and relocations), level
data (paths, cameras, volumes, rails), class dependencies, profiles, the names file.

## A class from high memory: the test

`RAC1_SESSION_HIGH_CLASS=<class>` turns a session's memory on and, at each level setup, registers the level's own class
`<class>` from a copy of its pool block in the session heap (0x04000000) instead of from the level's asset data:
everything the game and the renderers do with that class (relocation, registration, the mobys' class pointers, the
animations, the DMA chains into its packets, the modern renderer's lookup by header) then happens above 64 MB. With the
bolt crate (class 500) on Veldin:

- the reference picture (Veldin, frame 600, three crates in view) is the same with the modern renderer on GL and Vulkan
  and with the faithful renderer;
- walking to the first crates and breaking them with the wrench (33 bolts by frame 900): the same pictures at six frames
  and the same game state every 60 frames (Ratchet's place, state, bolts, health) as with the class at its native address.

`RAC1_SESSION_HIGH_CLASS=<class>:<id>` registers the copy besides the native class under a session class id (2048 and
up: the id map 0x197bc0 is filled for 0..2047 by the game, and zero past it to the next table) and places the level's
mobys of that class as `<id>`, its update function from the pool's row, the modern renderer drawing it with the class's
own file. The crates are drawn the same, but they do not behave the same: the engine tests class ids (a crate is a class
of 500 to 540: `moby_in_range_class_500`, used by the hero's attacks, the camera, the mines, the nanotech...) and the
crate's own code uses its id. A class brought into a session must keep its own id; a game class id is the same class on
every level (one exception, above), so a session needs another id only for a second copy of a class.

## What stage 2 builds on

- `ovl_data_to_veldin` and the extents for the contexts' classification; the extents are for the North American disc
  (another region's disc keeps the old rule).
- The pool's rows and blocks; `session_heap_alloc` for the blocks and the contexts' images.
- Classes keep their game ids in a session (see above); session ids only for second copies, with the engine's id tests in
  mind.
- The modern renderer's class lookup takes a session class's source class from `session_class_source`; another level's
  class and its textures need the import list (its textures are another level's files).
