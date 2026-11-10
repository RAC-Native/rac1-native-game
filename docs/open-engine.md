# The open engine

Work in progress, for developers. Nothing here is visible to players yet.

The goal is custom levels that run on the game's own engine with far more freedom than a level mod has today: any class of
any planet in one level, sections brought in and out while the game keeps running (no level load, no reset of the hero,
the weapons or the level's state), and more memory than the PS2's 32 MB. A custom level of this kind is a **session**:
the game loads one level, the session's base (Veldin's engine), and the host brings everything else in and out. The
original game is untouched: none of this runs unless a session is on, and every change is checked against the reference
pictures and the RAM hashes of the original's levels.

The work is staged; each stage ships on its own. This page describes what is real now. Stage 1 laid the foundations:
more memory for sessions, a heap above 64 MB, the engine's data told apart from a level's own, and the asset pool's index.
Stage 2 runs another level's class on Veldin's engine: a level's **context** (its own data copied and relocated, its
addresses translated, its own code handed out as tags), classes **imported** from the pool (drawn, heard), and the **twin
run** that compares such a class with itself on its own level. Neither ships a visible feature: their proof is the tests
below.

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
| `pool.idx` | the index, text: a `pool` line (version, disc), a `level` line per level (its counts and sizes), a `class` line per level and class: the block's place, size and hash, the class's update function and +0x2c word from the level's class table, its texture remap, its sounds (the level bank's sound of each of its sound definitions: the asset index's +0x70 list, read for as many definitions as the class block has, as the level setup reads it) |
| `context/levelNN.img` | the level's overlay data sections 0..5 as its overlay load puts them (0x15ef00 up to its code, a bss as zeros), with its code section's range: a context's image (below) |
| `context/levelNN.rel` | written by the game when it makes a level's context: each pointer word of the level's own data it relocated, what it points at and what it became |
| `sounds/levelNN.bnk` | the level's sound bank as its data file holds it (an imported class's sounds, as a host bank) |
| `sites/levelNN.txt` | the sites report (below), written by `tools/overlay_match/sites.py` |
| `pool.json` | the stamp: the version (4) and the disc; a pool made from another disc or by an older version is made again |

A class block runs to the next item of the level's asset data (the block and its padding). Two setups give the same bytes.
`rac1-native --setup --pool-report` prints the pool per level and in all; on the North American disc: 3641 class rows,
88.9 MB of blocks, 1350 class ids of which 291 are in more than one level and only one (class 2 on level 4) has other
bytes there; `classes.bin` is 1183 blocks, 41.7 MB.

`RAC1_POOL_CHECK=1` compares the pool with the game's own loads: each class the level setup registers against its row
(the block before relocation, the remap, then the update function and the word the registration gave), and the sounds the
level setup gives the classes' sound definitions against the rows' (`[pool] check level N: the sounds of ...`). Every class
of all 19 levels is equal.

Still to come in the pool (stages 3 and 4): level data (paths, cameras, volumes, rails), class dependencies, profiles, the
names file.

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

## Contexts: another level's code on Veldin's engine

Every level ships its own build of the engine, and its own classes' code is written against its own addresses: the
generated code names every overlay address of its level as `L<n>_ADDR(a)` (engine data included), the hand ports use
`OV()` for the engine's data and `L<n>_ADDR` for the level's own. In a session the loaded overlay is Veldin's, so level A's
addresses hold Veldin's things. A **context** for level A (`src/game/enhance/context.cpp`) makes A's code run there.

**The sites.** `region_site<N, a>` (`src/game/ovl/ovl_reloc.h`, behind every `L<n>_ADDR`) is cached until the translation
changes (`g_region_gen`: an overlay load, a context made or dropped), no longer fixed for the process. Without a context for
N, or with N's own overlay loaded, a site is `region_xlate`'s as before (the identity on the North American disc). With a
context for N it is the context's:

| A's address | What it is | It becomes |
|---|---|---|
| below 0x15ef00 | the base executable's | itself |
| in an engine object of A's map (the extents) | engine data | Veldin's object (or a view of A's own: below) |
| elsewhere in A's data sections | A's own data | the same place in A's context image |
| the start of a function A's map pairs with Veldin's | engine code | Veldin's function (its port runs), unless A has a port of its own for it |
| A's own code, or a label inside a function | A's own code | a tag |
| past A's code | A's run-time layout | itself, logged once |

**The image.** A's data sections (`pool/context/levelNN.img`, 0.5 to 0.75 MB) are copied into the session heap; the pointer
words of A's own data are relocated by the same table (`pool/context/levelNN.rel` records each). With the extents a level's
own data is small, and so is the relocation: Kalebo III's 93 words into its own data, 292 into its own code (its jump tables,
its classes' hooks and rows), 32 to Veldin's functions, none into engine data; Kerwan's 77, 375 and 80.
`RAC1_CTX_POISON=1` fills the image's engine objects with a pattern (a class reading the copy instead of the session's
object shows at once); `RAC1_CTX_TRACE=1` logs every site a context translates.

**Tags.** A's own code addresses overlap Veldin's code, so a context hands them out as tags: bit 30 set, A, and the address
(`ctx_tag`, `0x40000000 | A << 22 | (a - 0x1d0000)`), never a valid EE address in any session's RAM. The moby dispatch
(`ovl_dispatch.cpp`) finds a tag's port by (A, the address) among A's level functions; `class_hook_call_level` takes the
tag's level; `lvl_key` gives the address back, so A's `switch` over its `L<n>_CASE` labels works on its relocated jump
tables (the labels inside a function are tags, also inside a function the map pairs with Veldin's).

**Views.** `ctx_view(A, object, size, copy)`: an engine object A's sites see as a copy of A's own while the session's
engine keeps its own (DESIGN 6.2's per-context level data, its first form). The twin run gives Kalebo III's robot its
level's paths this way.

**What is not done yet:** the context init (A's own data its level start writes: no class tried so far needed one), the
current level (0x15ed84 stays the base's: hand ports' `on_level` tests take the base's branch), the other region's discs
(contexts are for the North American disc: the region layer composes with them in a later stage).

## Classes imported from other levels

`RAC1_SESSION_IMPORT=16:1445,3:455` (testing; a mod's `import` line comes with the session runtime) turns a session's
memory on and, at each level setup, imports each class: its pool block copied into the session heap and registered by the
game's own `moby_class_register` under its **own class id** (a class the base has already gets a session id, for a second
copy), its level's context made, its update function and +0x2c word (its hook table) translated by the context, its sound
definitions pointed at its level's bank, loaded as a host bank (`snd989_host_bank_load`: the whole bank for now). A moby of
it is placed or spawned as any other.

**Drawing.** The modern renderer has an **import list** (`mrp_moby_import`): the imported classes' own class files
(`mrp/moby/levelNN/<class>.mrpm`), their textures appended to the level's (as the debug hoverboard's guest class). A moby of
an imported class is matched by its header and id as any other; its source (level and class) comes from the session. The
faithful renderer draws imported classes wrongly (their texture slots name the base's texture 0): sessions are for the
modern renderer.

Kalebo III's robot (class 1445) imported into Veldin: drawn on GL and Vulkan, walking, attacking Ratchet, its sounds played
from Kalebo III's bank, hit by the wrench and the Blaster until it dies and drops its bolts.

## The twin run

The acceptance test of an imported class: the same class on its own level and in a session, from the same state, compared
frame by frame (`src/game/enhance/twin.cpp`, `tools/twin_compare.py`).

```
# on its own level: the hero put next to it, the moby taken at frame 1110 (when the hero stands)
RAC1_HEADLESS=1 RAC1_LEVEL=16 RAC1_HERO_AT=1445@1100,4,0,1 RAC1_FRAMES=1710 RAC1_SESSION_TWIN=16:1445@1110 \
  RAC1_TWIN_FILE=cap.bin RAC1_TWIN_LOG=native.twl RAC1_TWIN_ALONE=1 RAC1_TWIN_PATHS=1 ./build/rac1-native
# in a session on Veldin: imported, spawned from the capture on Veldin's ground
RAC1_HEADLESS=1 RAC1_GOTO=0@60 RAC1_FRAMES=1710 RAC1_SESSION_TWIN=16:1445@1110 RAC1_TWIN_FILE=cap.bin \
  RAC1_TWIN_LOG=session.twl RAC1_TWIN_AT=150,122,29.484 RAC1_TWIN_ALONE=1 ./build/rac1-native
tools/twin_compare.py native.twl session.twl
```

- The native run takes the class's live moby nearest the hero, and the mobys its record and pvars name, when the hero
  stands still; with `RAC1_TWIN_PATHS=1` the level's paths. The session run imports the class (and those mobys' classes),
  puts the hero at the same place relative to it, and a frame later spawns them from the capture: their records and pvars
  with their pointers moved to the session's (the class block, the pvars, the hero, the level's context), the places near
  it moved with it, the paths a view of its context's own.
- Both log the twin every frame: its record, its pvars, the hero's place, health and state, the mobys its update spawned.
  Its updates draw their random numbers from a seed of their own in both runs (the two levels' other mobys draw theirs in
  their own order); `RAC1_TWIN_ALONE=1` holds the levels' other mobys.
- `tools/twin_compare.py` compares the class's words (places relative to each run's origin, addresses by what they name;
  the update chain link +0x28 and the collision stamp +0x9c are the engine's bookkeeping) and the hero's apart.

Results:

| Class | Frames | The class | The hero |
|---|---|---|---|
| Kerwan's 455 (no level data: a stationary object; Ratchet swinging the wrench beside it, which it ignores in both runs) | 600 | equal in all 600 | his place within 0.01 (the two floors), his swings' states in 19 frames (Kerwan's hero build is Novalis's) |
| Kalebo III's robot 1445 (reads a path), attacking | 583 | equal for 33 frames: the approach and the first hit (health 4 to 3 in both) | his knock-back differs from frame 33 (Kalebo III has its own hero build); then the robot follows a hero a few centimetres elsewhere: its second hit in both at frame 160 |

Without its level's paths the robot gives up at its first update (its pvars +0x1c4 = 2): the twin found the level data it
reads at once, and with the paths seen through its context's view it plays as on its own level.

Long runs (`RAC1_TWIN_RESPAWN=150`, the robot spawned again 150 frames after each death, Ratchet put in front of it every
400 frames): 7 900 frames with the wrench and 7 900 with the Blaster, 20 robots killed in each, no crash.

## The sites report

`tools/overlay_match/sites.py` lists every level-only address the ports name (the generated code's and the hand ports'
`L<n>_ADDR`) as a context classifies it, with the jump tables the generated code reads, into `pool/sites/levelNN.txt`; on
the North American disc 333 (Gaspar) to 967 (Veldin's finale) addresses a level. `--check-rel` checks that a context's
relocation tagged every jump table's entries (Kerwan 137 of 137, Kalebo III 159 of 159).

## What stage 3 builds on

- `session_import_add` and the imports' registration; `ctx_create`, `ctx_xlate`, `ctx_view`; `mrp_moby_import`.
- The twin run for every class brought in: it finds what a class reads of its level at once (the robot's paths).
- The classes an import needs with it (the dependency closure: a projectile, an effect, an attached moby) are not imported
  yet: the twin run takes the mobys a class names, but a class it spawns must be imported by hand.
