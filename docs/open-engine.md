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
below. Stage 3 completes the pool: a class comes with what it spawns (its **dependencies**), its sounds, and it can be
**unloaded** during play; contexts work on any base (Kalebo III's, a level mod's); a level mod brings classes of other
planets with **`import` lines** and its plugin spawns them (the first part level-mod authors can use). Stage 4 gives each
context its own **level data** (paths, groups, cameras, volumes, rails: a registry), describes classes in **profiles** (which
of their values are level data, what a spawn gives them), and brings in enemies that read their planet's data: six of them,
from six planets, fight in the arena example's test copy. Stage 5 makes a level mod a **session** (`level.format 3`): the
level is loaded once, at the arrival, and a death is a **soft reset** (Ratchet back at a checkpoint, no level load, the
weapons, the ammo, the bolts, the plugins' state and their objects kept); a save made in a session brings the player back
into it. Stage 6 splits a session into **sections** (each its own collision, meshes, sky, lights, music and objects) switched
by **portals** in one frame, streamed in on a worker thread and committed a little each frame, with no level load. Stage 7
adds **grind rails** (level.txt's, per section; the hero code's landing gate skipped), **items for the visit**, tested
**Magneboots surfaces** and the **set-piece kit** (a fade, a camera service, hazards, moving platforms Ratchet rides,
classes with a collision mesh of their own), the SDK's ABI 7 room filled (ABI 11). Stage 8 makes a section a **game planet's
world** (`world <level>`): its scenery drawn by the game's own world passes and the modern renderer from that planet's
assets, its collision, sky, lights, fog, music and level data the engine's while Ratchet is in it, and the planet's own
objects a mod chooses placed with their uids mapped to the session's, running on their planet's code. Stage 9 adds **hero
contexts**: another planet's own hero code and camera code run Ratchet in a section (Kalebo III's, which rides the race's
hoverboard with its race camera), and the game's own **hero modes** (Clank, Giant Clank, the board) are entered from a
section's `hero` line or a plugin (ABI 12); a base whose hero lacks an animation gets it from Veldin's core.

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
| `placements/levelNN.bin` | each class's first placement in the level's gameplay file (its record and its pvars, the pvar words that are offsets into its own pvars, name other mobys or point into the level's shared pvar data): a session spawns an imported class from it |
| `deps/levelNN.txt` | each class's dependencies (below), written by `tools/overlay_match/deps.py` |
| `leveldata/levelNN.bin` | the level's gameplay file, unpacked (the disc's region's): its paths, groups, cameras, volumes, rails and placements, for a context's registry (below); made from the disc the first time a session needs that level's |
| `sites/levelNN.txt` | the sites report (below), written by `tools/overlay_match/sites.py` |
| `pool.json` | the stamp: the version (5) and the disc; a pool made from another disc or by an older version is made again |

A class block runs to the next item of the level's asset data (the block and its padding). Two setups give the same bytes.
`rac1-native --setup --pool-report` prints the pool per level and in all; on the North American disc: 3641 class rows,
88.9 MB of blocks, 1350 class ids of which 291 are in more than one level and only one (class 2 on level 4) has other
bytes there; `classes.bin` is 1183 blocks, 41.7 MB.

`RAC1_POOL_CHECK=1` compares the pool with the game's own loads: each class the level setup registers against its row
(the block before relocation, the remap, then the update function and the word the registration gave), and the sounds the
level setup gives the classes' sound definitions against the rows' (`[pool] check level N: the sounds of ...`). Every class
of all 19 levels is equal.

The level data is in `leveldata/` (stage 4, below); the profiles are in the repository (`data/pool/profiles`).

**Names.** `data/pool/names.txt` (in the repository) gives a class of a level a stable name (`umbris.turret` is Umbris's
class 1041), for level.txt's `import` lines and the plugins; a class without a name is `<level>:<class>`. The file starts
with the classes the stage 3 tests use; a name never changes once given.

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

**Any base.** A context translates a level's engine data and paired functions to the session base's, through the base's
map (as the ports' `OV` / `OVF`): Veldin's objects on Veldin's engine, Kalebo III's on Kalebo III's (a level mod on
Kalebo III's code). Views (`ctx_view`) are keyed by the level's own addresses (Veldin's object found in the level through its
extents, word by word): they hold on any base, and on a level whose object Veldin's map does not pair one to one (Kalebo
III's and Rilgar's rail table is at 0x15f71c, Veldin's at 0x15f70c).

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

## Dependencies

A class spawns other classes: its shots, the effects of its hits and death, its debris, the drops; the engine spawns some
for it (the bolts, the common explosions). A session that imports a class imports what it spawns with it: its
**closure**, each class from the import's level unless the base has it (a class id is the same class on every level, so the
base's own serves).

`tools/overlay_match/deps.py` finds a class's dependencies in its code: its own functions (the generated code of its level,
from the functions named for it, the port registered for its update function, the functions of its hook table), the class
ids each loads into a spawner's class argument (`moby_spawn`'s, and the engine's spawners found by their parameters:
`debris_spawn`, `chunk_spawn`...), then the engine's functions it calls and what they spawn. Only ids of classes the level
has are kept (a constant that is no class of the level is a value). It writes `pool/deps/levelNN.txt`:

```
class 1041 own 1,2,10,882,1569,1570 engine 13,14,15,16,112,122,639 seen - reads - fns 2 sounds 2
```

`own`: what its own code spawns; `engine`: what the engine spawns for it; `seen`: what the game was **seen** spawning and
the code computes (an id from a table, a base plus a count), from `data/pool/spawns.txt` (in the repository, measured);
`reads`: the level data its sites name (path, group, camera, volume and rail tables: stage 4's work); `sounds`: its sound
definitions. The composer runs the tool when a level mod's `import` lines need it and the pool has no report yet.

**The probe.** `RAC1_DEPS_PROBE=1` logs every class a moby's update spawns, by the spawner's class, on the game's own levels.
Every level walked for 4000 frames (`shipwalk`): 226 spawner-spawned pairs, 112 of them found by the static search (the
others: the engine's own classes' code, the ported crates' and the hero's, ids computed from tables or counts, a class
spawning copies of itself); `deps.py --probe <logs>` merges them into `data/pool/spawns.txt`, so the closure has both.

**The spawn watch.** In a session with imported classes, `moby_spawn` asks the session first: a class the session lacks is
refused (as when the moby array is full: the game's code handles 0) and logged once per spawner and class
(`[session] a moby of class C (L:C) spawned class D, which the session lacks: refused`): a dependency the closure missed
shows, and nothing runs on an unregistered class's index.

## Sounds

Each imported class's sound definitions point at its level's bank, loaded once as a host bank (the sound driver keeps its
own copy in host memory: the IOP's 2 MB are not involved) and shared by the level's classes; it is unloaded with the
level's last class. Whole banks, not per-class subsets: a level's bank is 1.1 to 1.8 MB of host memory (Umbris 1765 KB,
Orxon 1739 KB, Gemlik 1693 KB), loaded in a few milliseconds, and a subset would need a bank rewritten per session.
After the imports, `[session] sounds: N imported classes, M definitions on their levels' banks, 0 differ from the pool's:
ok` checks every definition. In the stage 3 level mod run (`RAC1_SND_TRACE=1`) the game played 222 sounds from Umbris's
bank and 24 from Orxon's.

## Unloading

`session_import_unload(level, class)` (a plugin's `import_release`) drops an import: it and the dependencies no other
import needs go. Their mobys are retired (the game's `moby_retire`) at once and their class table entries cleared (the class
id map, the header, the update function, the +0x2c word); the class index is given back and taken first by the next
registration, so the 224 entries of the class table only ever hold what is resident. Their heap blocks (the class block,
its texture remap), and their level's context and bank when no class of the level is left, are freed three frames later
(the renderer's thread may still draw the frame before from a class block). The modern renderer's import list is shortened
(from stage 4 without loading the level's moby assets again: below). `session_import_load` brings a class (and its
closure) in during play.

`RAC1_SESSION_CYCLE=umbris.turret@1500,100,12` on Kalebo III: 100 rounds of unload and load again, two turrets spawned
after each load: the heap (0x142680 bytes in 16 blocks, one free run), the class table (198) and the renderer's import
list (6 classes, 239 moby textures) end as after the first round.

At a level setup every pool class, the twin's blocks and the spawns' pvars are freed.

## The session's memory accounting

`[session] memory: ...` after each change (the imports registered, an unload's frees):

```
[session] memory: 11 classes imported (1157 KB), 6 contexts (3943 KB), their level data 504 KB, other blocks 0 KB
(spawned pvars, the twin's); heap 5604 KB used, 191003 KB free in 1 runs; 6 sound banks 10251 KB (host memory); class
table 203 of 224 (0 indices free); the MRP's import list 11 classes
```

A context image (0.5 to 0.75 MB) is the largest part of an import; the classes themselves are a few KB to a few hundred.

## Level mods: `import` lines

A level mod's `level.txt` names classes of other planets (format 2 for now: on the base level's code):

```
import umbris.turret
import 10:939
```

The composer (`rac1-native --setup --level-mod`) resolves each (a name of `data/pool/names.txt`, or `<level>:<class>`),
makes the pool and the dependencies report when there are none, computes the closure (the base level's classes serve
themselves) and writes it to the build's `imports.txt`; `compose.log` says what each import spawns and reads. When the
level's header is read the game reads `imports.txt`, turns the session's memory on and registers the imports at the
level's setup, after the base's classes: Kalebo III's robot is the base's own, the imports come with their contexts.

The plugin (ABI 6, [The mod SDK](sdk.md)) spawns them: `import_class(name)` gives the class id in the level,
`import_spawn(id, pos, yaw)` starts one from its first placement on its level (`session_import_spawn`: its scale, its
settings and pvars; the mobys its pvars name and its level's shared data none, its pvars in the heap when they are larger
than a spawn's 0x80 bytes, freed when it is gone), `import_load` / `import_release` bring a class in or take it out.

What a level mod gets: classes that read no level data of their own level (`reads -` in the report) as they are; classes
that do read it (paths, areas, boxes) find their planet's in their context's registry, and their profile gives a spawn
level data of its own round where it is spawned (stage 4, below).

## Stage 3's classes and their twin runs

The twin run on Kalebo III's engine (`RAC1_LEVEL=16`, `RAC1_TWIN_AT` a place by the landing pad):

| Class | Its closure | The twin run | In the level mod |
|---|---|---|---|
| Umbris's turret 1041 (`umbris.turret`: fires at Ratchet, no level data) | its shot 882 (whose explosion is the engine's 112, 639: the base's), its debris 1569, 1570 | equal for 3 frames, then its aim follows a hero a few hundredths of a unit elsewhere (Kalebo III's hero build): the same behaviour, its shots at +3, +10, +17... in both runs, 26 by +180, Ratchet hurt in both | fires at Ratchet (4 to 2 health), its shots explode, its sounds from Umbris's bank |
| Orxon's spore plant 939 (`orxon.spore_plant`, no level data) | its pod 938 (bursting into the engine's 112, 639) | equal in all 601 frames (its record, its pvars, its spawns: the pod at +295 in both) | spits pods; released (with its pod: two classes, Orxon's context and bank freed) and loaded again during play |

Two corrections to the harness made these comparable: the session's frame clock (0x15f5cc) is set to the native run's at
the capture (classes keep times of it in their pvars: the turret's next shot), and the twin's moby group (+0x21) is none
(the session has none of its level's groups, and a moby of a group is updated only through its group's list: the turret
was never updated). The robot of stage 2 (Kalebo III's 1445) runs as before; on a Kalebo III base it is the base's own.

## Level data per context: the registries

Most enemies read their planet's level data: a path they patrol or fly along, an area (a path's points as a polygon) they
watch, a box that wakes them, a group of mobys they act with. The engine keeps these in its objects: the path table
(0x1b04b0, 176 entries, its count 0x160104), the groups (0x1ab840, 112), the cameras (0x15ef50 / 0x15ef54), the volumes (the
0x90-byte records 0x1600e4 and three lists of 0x80-byte records 0x1600ec..0x160100: `point_in_box` reads the first), the grind
rails (0x15f70c / 0x15f710). In a session each context has its own copy, a **registry** (`src/game/enhance/leveldata.cpp`):

- **Made with the context** (the first import of a level), in the session heap (owner `level data N`): the level's own
  paths, cameras, volumes and rails as its load lays them out, from the pool's `leveldata/levelNN.bin`, at the same indices
  as on the level; its groups none (they name the level's own mobys); its shared pvar data (below) copied.
  `[ld] level 17's registry at 04088b00 (0x118d0 bytes): 43 paths, 9 cameras, 0 volumes, 0 rails of its own ...`
- **Its sites see it.** Views of the context (`ctx_view`) for each object: level A's code naming the path table reads A's.
- **So do the engine's functions it calls.** Many classes give an engine function an index, not a pointer
  (`path_crossing`, `path_push_out_2d`, `path_crossed`, `point_in_box`): the engine reads the base's objects. Around the
  update of a moby of an imported class, the moby dispatch swaps the registry's objects into the engine's and back
  (`ld_enter` / `ld_leave`: about 300 words each way); what the engine wrote meanwhile goes back into the registry.
- **No index competition.** Each registry's indices are its own: what a mod or a profile adds is taken from the table's end
  down to the level's own count (`ld_path_reserve`), and given back when the moby it was for is gone.
- **The base's own classes** (Kalebo III's enemies on a Kalebo III level mod) read the base's objects, the session's view:
  what is added there goes into the path table's free entries from its end, its points in the mods' block of memory
  0xdf000..0xe0000, as the SDK's ABI 5 did.

**A spawn's parts and shared data.** `import_spawn` starts an object from its class's first placement on its planet (the
pool's level data): the mobys its values name (an attached part, a carried trooper) are spawned with it, placed and turned
as they are relative to it on its planet, their classes brought in with the import's closure (`named by ... (a part)`);
its pointers into its planet's shared pvar data point into the registry's copy. A profile that gives such a value a default
of its own (none, a value) overrules the part.

## Class profiles

`data/pool/profiles/L<level>_<class>.txt` (in the repository: ids and offsets only, our knowledge of the class) says which
private values (pvars) of a class are level data and what a spawn gives them; `src/game/enhance/class_profiles.cpp` reads
them:

```
class 16:1356 the dropship (flies in, drops jetpack troopers, shoots class 50; update 0x2e22d8)
pvar_size 0x1a0
slot 0xc0 moby   trooper_a  # the troopers it carries (moby indices, -1 none)
slot 0xd0 area   fight      # with no troopers it stays and fights what target_select finds here (-1: never fights)
slot 0xd8 path   arrive     # its arrival
slot 0xdc path   leave      # its leave (-1: it stays)
default trooper_a none
default fight none
default arrive points lift 9 -60 0 25, -32 0 12, -12 0 3, 0 0 0
default leave points lift 9 0 0 0, 15 0 6, 45 0 25, 90 0 50
option troops host kal_dropship_troops
hunt object host kal_dropship
```

| Line | Means |
|---|---|
| `slot <offset> <kind> <name>` | a pvar word: `path`, `area` (a path's points as a polygon), `group`, `moby` (another moby's index), `word`, `vec3` |
| `default <slot> none` / `value <n>` | at a spawn: -1 / the word |
| `default <slot> world` | the whole level's area (a square of 4000 round the origin, one a registry, shared) |
| `default <slot> square <r>` | a path of its own: a square of `r` round where it is spawned |
| `default <slot> points [lift <h>] <ahead> <left> <up>, ...` | a path of its own: points relative to the spawn in its yaw (`lift` raises the spawn's point first) |
| `option <name> slot <slot> <min> <max>` / `option <name> host <fn>` | what the SDK's `base_option` may set |
| `hunt object [host <fn>]` / `hunt moby host <fn>` | what `base_hunt` does: an object of a spawn pool, or any moby of the class |
| `frame host <fn>` | a host behaviour run after each frame's updates |

Host behaviours are the C++ data cannot say, registered by name in `class_profiles.cpp`: the SDK's ABI 5 fixes for Kalebo
III's enemies moved there unchanged (`kal_bomb_bot`, `kal_gadgetron`, `kal_trooper`, `kal_dropship`, `kal_dropship_troops`,
`kal_robot_guard`). Profiles serve the base level's classes started by `base_spawn` (their paths in the base's objects) and
the imported classes `import_spawn` starts (their paths in their context's registry); a plugin rebinds an imported object's
slots by name (SDK ABI 8: `object_bind_path`, `object_bind`, `group_create`, `object_slots`; level.txt's `path` lines). A
class without a profile is imported as before: its values as its first placement has them, indices into its own planet's
level data (its registry has them, at their places on its planet).

**The ABI 5 enemies on profiles, checked.** The arena example (a copy composed under `s4-night-arena`) run through six of
its plugin's test hooks before and after the move (`RAC1_MOD_TEST=arena_ch0k` the robots, `arena_ch1k` the Gadgetron robots,
`arena_ch2k` the dropships and troopers, `arena_ch10k` every kind, `arena_ch11k` the gunship and the bombs, `arena_course`
the grind course; 5 200 to 9 000 frames each): the same RAM hash in all six (`4c521f97`, `ab6c6324`, `f4ea2ab7`, `2cc9f4d5`,
`879ae774`, `cc7934da`), the challenges won as before.

## Stage 4's enemies

Each from another planet, its profile and name made, accepted by the twin run on Veldin's engine (the session spawned where
Veldin's floor puts the hero as on its planet) and then fought in the arena's test copy (`s4_enemies`: the six imported by
`import` lines and spawned by its plugin in front of Ratchet, the wrench swinging):

| Name | Class | Its level data (profile) | The twin run | In the arena |
|---|---|---|---|---|
| `fleet.blarg_trooper` | 17:1382 | an area, four boxes that wake it (`point_in_box`: the volumes moved with the twin) | the same states at the same places (5 at +120, 11 at +300), Ratchet hit at +300 in both; its words differ from +1 in the heights it reads of its floor | wakes, shoots, hits Ratchet |
| `gemlik.flying_robot` | 13:63 | two paths (none on its planet) | the same states and places, Ratchet 4 to 3 to 2 at the same frames; the words differ in two floor heights | flies at Ratchet, killed by the wrench |
| `gaspar.walking_robot` | 9:193 | a path (none on its planet) | the same states, Ratchet 4 to 3 to 2 at +120 / +300 in both, killed in both | walks at him, killed |
| `blarg.serpent` | 6:1048 | its pen (an area it is kept inside: `path_push_out_2d`) | equal 46 frames, then the same states, Ratchet hit in both (4 to 3) | stays in its pen (bound to level.txt's `path pen` by the plugin), strikes |
| `pokitaru.amoeboid` | 11:572 | two areas (one path on its planet) | its pvars equal 153 frames; Ratchet hurt in both | comes at him, killed |
| `hoven.flying_trooper` | 12:294 | a patrol, an area, three lines | equal in all 600 frames, but idle in both: it does not wake for a hero placed beside it on its own level either | spawned, killed by the wrench |

In the arena run (5 400 frames): every class drawn on the modern renderer, Ratchet hurt down to 0 by the second wave, no
refused spawn, no unported call.

## Importing during play: the renderer

A change of the modern renderer's import list (an `import_load` or `import_release` during play) reads only the new classes'
files and uploads their textures into texture arrays of their own; the level's own classes, textures and vertex data stay.
`RAC1_SESSION_CYCLE=umbris.turret@1500,100,12` on Kalebo III: each load 1.1 ms (it was 22 to 30 ms, a reload of the level's
moby assets); a release keeps its textures' layers (unused) until more than 256 are left behind, then the level's assets are
loaded again once.

## Sessions (stage 5)

A level mod of **format 3** (`mod.json`: `"level": { "format": 3 }`) is a session (DESIGN section 2): the game loads its level
once, at the arrival (the ship's flight and landing, as any level mod's), and never again until the player leaves (the ship,
a save of another level loaded). Its base is Veldin's engine by default (`level.base` 0, DECISIONS 1); the other bases of the
allow-list stay possible (`level.base` 3, 4, 16: the arena's test copy runs on Kalebo III's, whose enemies are its own).
Formats 1 and 2 are unchanged: nothing of the session runtime runs unless a format 3 level is current (`g_session_rt`, the
one test of every hook; `src/game/enhance/session_runtime.cpp`).

### The level

The composer (`rac1-native --setup --level-mod`) composes a format 3 level as a format 2 one, with:

- **level.txt's session lines** (refused below format 3, with the line's number):

  | Line | What |
  |---|---|
  | `session` | the level is a session (mod.json's `level.format 3` decides; the line says so in level.txt) |
  | `checkpoint <name> <x> <y> <z> [<yaw>]` | a place Ratchet comes back to after a death (the level's offset applies). `start` is where the session starts him; without one, the `hero` line's place (or `hero auto`'s) is `start` |
  | `respawn reset` / `respawn keep` | the level's placed objects at a respawn (below); `reset` by default |
  | `moby <class> <x> <y> <z> [<yaw>] [<slot>=<value> ...]` | a placed object as in format 1, its class's profile's slots set at the arrival: a `path` or `area` slot takes the name of a `path` line (its points become a path of the base's own, in the session heap), the other kinds a number or `none` |

- **the moby array's room for spawned objects** as large as the base level's placed objects leave free (the array no larger
  than the base level's own, at most 2047 mobys: the game's per-moby tables have 0x7ff entries; their 0x80-byte pvars in the
  room the base level's placed pvars took): `[compose] session: room for 478 spawned objects (the base level's 256, and 222
  its placed ones leave)` on Kalebo III, 545 on Veldin.
- **`session.txt`** in the build: the checkpoints, the policy, the moby lines' slots (`bind <uid> <class> <slot> <value>`).
- **the profiles' check** (all formats): a `moby` line's slots against its class's profile (`data/pool/profiles`), an
  `import` of a class that reads its planet's level data without a profile. Warnings in `compose.log`, the level is composed:

  ```
  [compose] profiles: warning: level.txt:135: class 1445 has no slot "patrol" (its profile's: given_up, guard, target): not set
  ```

### The arrival

When the level's header is read (`level_slots.cpp`) the build's `session.txt` turns the session on (its memory too:
`[session] the session "night-arena": 2 checkpoints, respawn reset; no level load until it is left`). At the end of the
level's gameplay load (after a level mod's spawn pools are taken out of play), the session keeps what its soft resets need:
the hero's moby record as the load placed him, and each placed object alive then (its record, its pvars, its placement's
spawn rule read from the gameplay file still in memory); the `moby` lines' slots are set first.

### Death: the soft reset

The game restarts a level when Ratchet has died (`overlay_entry.cpp`: the hero's respawn flag +0x20b1, then
`level_setup(0, 1)`: the gameplay file read again, every moby of the level rebuilt, the plugins' level started again). In a
session `session_respawn()` runs instead, between the same sound stops before and the music and HUD after:

1. **The hero**: his moby put back as the arrival placed it, at the current checkpoint (its place and yaw), then as the end
   of a gameplay load starts him (`hero_init`: the player block cleared, his health full, dropped onto the floor there;
   `hero_level_reset`; the camera's view cleared, `camera_init`, `camera_update`). His moby leaves the moby grid and comes
   back into it where he stands; its update chain's link (the frame's bookkeeping) is kept.
2. **The level's placed objects**, by the policy: `reset` puts back as placed each one killed since the arrival, unless the
   level's records keep it killed for good (the game's spawn rule: the checkpoint's state byte, the missions' rules, the
   level's own bits: a crate broken, a pickup taken). The game's restart also keeps an enemy killed "for the visit" dead (its
   placement's flag 8); a session brings it back (DECISIONS 4: a battle's enemies come back). The living stay as they are.
   `keep` leaves them all.
3. **The hero's spawns** (done first, before his state is cleared): a level's start clears the whole moby array, so no moby the
   hero made survives it. The session remembers, from the arrival, every moby spawned while the hero's frame runs
   (`hero_frame_update`: his equipment, the weapon in his hand, the pack, helmet and boots, the effects his states make) and
   every moby spawned by one of those in its update (the Glove of Doom's hand's agents, the Mine Glove's mines, the Drone
   Device's drones, the Decoy, the Taunter, the Visibomb, the Devastator's rocket and the other projectiles): the moby's
   spawner is the moby whose update runs (`g_spawn_updating`), and the hero's frame names his moby. The soft reset takes them
   all out (`moby_retire`, after giving back the animation layers a moby holds in the 6-entry pool at 0x18eb40) and gives back
   the hero's own layers; `hero_init` then clears the player block and the game spawns his equipment again. Before this the old
   ones stayed beside the new ones: frozen agents (class 230, state 3) where he died, two hand mobys and two packs, the old hand
   running its aim code on the reset hero (the arms up, broken animations), more agents spawned until the moby array ran out.
   The plugin's own objects, the placed ones and the imports are not the hero's: they are kept. The log line:
   `[session] the soft reset takes out the hero's spawns: 21 mobys (186 x8, 229 x1, 230 x9, 601 x1, 607 x1, 1204 x1)`.
4. **Nothing else**: the weapons, the ammo, the bolts, the items, the session's imports and contexts, the plugins' state and
   the objects they started (base level objects, imported ones, the level's own classes), the spawn pools, the frame clock.
   No level load (one `[levels] level 16's header` line for the whole visit).
5. **The plugins' `respawn` callback** (SDK ABI 9) with the checkpoint: what the battle needs back is the plugin's to decide
   (the arena's test copy starts the wave Ratchet died in again, its enemies back).

```
[session] respawn 1 at checkpoint "arena" (176.00 168.00 104.50): no level load; placed objects reset: 2 back, 0 killed for good
[mod s5-arena] respawn 1 at "arena" (176.0 168.0 104.5): challenge Primo sangue goes on, wave 0 again (4 of its enemies removed), health 4
```

**Checkpoints.** The current one is `start` at the arrival; a plugin chooses one of level.txt's (`checkpoint_set`) or
makes one (`checkpoint_at`); the game's own checkpoint saves (0x284e50: an imported class's own, as the Snagglebeast's)
make theirs the session's (`game`).

### Saves

A memory card save made in a session is the game's own save, byte for byte, as any level mod's (the base level's records,
the level number the game wrote). `save<N>.pc` gets, beside the level mod's item (0x200: its records) and the plugins' (0x300),
the session's item (0x201): its level's id, its sections (one now, `main`: the current checkpoint's name, place and yaw, the
respawns so far, the placed objects gone for good), and each plugin's session bytes (the `session_save` callback, ABI 9).
Loading the save brings the player back to the level (the level mod's own rule), and the session to its checkpoint: once the
arrival's landing is over Ratchet is put there, and the plugins get their bytes after their `level_start`
(`session_load`). What the game never saves (the mobys' places, their pvars) a session does not save either: a challenge
running at the save is not resumed. A session whose mod is off or missing is written back as it was read (the planet prompt
of level mods applies).

```
[session] the save's session item: "night-arena", checkpoint "arena", 0 placed objects gone, 1 plugins' bytes (100 bytes)
...
[session] the save's session: checkpoint "arena" (176.00 168.00 104.50), 0 respawns, 0 placed objects gone (0 taken out now), 1 plugins' bytes
[mod s5-arena] session_load: the save was made in challenge Primo sangue (wave 0): the menu on it
[session] Ratchet at the save's checkpoint "arena" (176.00 168.00 104.50)
```

**Leaving.** The ship (or any level change) reads another level's header: the session is off
(`[session] the session "night-arena" left (5 respawns)`), the base's own paths and groups in the heap freed. The session's
memory stays on (stage 3's call): the game's own levels play as before with it. A session is a planet of the ship's list
like any level mod (DECISIONS 8: an extra planet, before the ending; it never ends the game).

### The base's view in the session heap

The base level's own objects (the session's view of the engine's level data: the base level's classes started by
`base_spawn`, their profiles' paths; `moby` lines' slots) put the paths they add, and the groups `group_create` makes for
them, in the session heap (owner `level data base`, given back when the session is left; a path's slot given back when
its object is gone) instead of the mods' 4 KB block at 0xdf000, which formats 1 and 2 keep using as before.

### Groups

- `group_create` (ABI 8) takes the base level's own objects too (a `base_spawn` object of a class with a profile, ABI 9's
  game): the group in the base's objects (the session heap in a session, the mods' block otherwise), for `object_bind`,
  which, like `object_bind_path` and `object_slots`, now serves those objects as well as imported ones.
- **The twin run captures groups**: the level's moby groups the twin is in or names (its +0x21, its profile's `group`
  slots), their members taken with it (up to 16 mobys in all); the session run makes each again at its own index in the
  twin's context's registry (`ld_group_set`), of the session's mobys. Gaspar's walking robot (9:193, `reads group:1`) on
  Veldin's engine: `[twin] native: group 17 (3 members, 2 taken with it): its own (+0x21)`, `[twin] session: group 17 of
  level 9: 3 of its 3 members in its context's registry`; its words (record, pvars, spawns) equal in all 313 frames
  compared, the hero within 2 mm (the two floors). The capture file is `TWN4` (TWN3 files are still read).

### Testing

```
# 50 deaths mid-challenge with the six imported enemies round the arena (the arena's test copy, format 3)
RAC1_HEADLESS=1 RAC1_MODS=1 RAC1_MOD_CODE=1 RAC1_GOTO=night-arena@60 RAC1_MOD_TEST=arena_s5die0 RAC1_FRAMES=14500 ./build/rac1-native
# a save in the session (the card's slot 2), then loaded from another planet
RAC1_MEMCARD_DIR=<card> ... RAC1_MOD_TEST=arena_s5save RAC1_SESSION_SAVE_AT=2400:2 RAC1_FRAMES=3000
RAC1_MEMCARD_DIR=<card> ... RAC1_GOTO=3@60 RAC1_SESSION_LOAD_AT=1200:2 RAC1_FRAMES=5000
```

`RAC1_SESSION_SAVE_AT=<frame>:<slot>` saves as the pause menu's Save page does (the card's summaries read, `save_game_image`,
the card's operation 0x13; `save<N>.pc` with it), `RAC1_SESSION_LOAD_AT=<frame>:<slot>` loads as its Load page does (the
card's operation 0xd, then the level left for the save's). A run started by `RAC1_GOTO` never read the card, so the port's
record still says "unchecked" and the game refuses to save over it: the test marks it read, as a game loaded from the title
has it. Such a run had no save in use either, and the game shows its once-a-boot card notice (screen kind 5: game mode 4
until Cross, after 2 seconds) the first frame there is one, on any planet but Veldin: right after the test's save, on a level
mod on Kalebo III's base, for ever in a headless run without input (a plateau on Veldin's base never showed it). The test
marks the notice shown too, as a game loaded from the title showed it at its first planet. A save made leaving a planet for
the session's level (the ship, `RAC1_GOTO`) has no session item 0x201: it is made before the session's arrival, and a
load of it starts the session at its arrival as any level mod's.

Results (headless, the arena's test copy `s5-arena`: Kalebo III's base, the six imported enemies of stage 4, two placed
robots; and a copy of the example level, `s5-plateau`, on Veldin's engine with Kalebo III's robot imported):

| Test | Result |
|---|---|
| 50 deaths in "Primo sangue" (`arena_s5die0`, 14 500 frames) | 50 soft resets, one level load in the whole run; the wave started again each time (its enemies back), the two placed robots removed at +700 back at the next respawn, the imported enemies kept; Ratchet back south of the hub with his health full, the camera behind him; no crash, no refused spawn |
| The session left and entered again (`RAC1_GOTO=night-arena@60,3@3500,night-arena@6500`) | the session off at Kerwan's header (5 respawns), Kerwan playing, the session on again at the return, 10 more respawns there at `start` |
| A save in the session, loaded on Kerwan | the card's save (the game's bytes), the session's item (checkpoint `arena`, the plugin's 16 bytes); the load back to the arena, the plugin's bytes after its start, Ratchet at `arena` |
| Veldin's engine (`s5-plateau`, `s5_robots`) | three of Kalebo III's robots (imported) and the level's sentries kill Ratchet: a respawn at the checkpoint `pad` |
| The original | the reference pictures and RAMHASH unchanged; the arena's format 2 regression (six challenges, no import lines) unchanged |

## Sections (stage 6)

A session can be made of **sections** (DESIGN section 7): parts of the level with their own collision, meshes, sky, lights,
music, fog, background, death height and objects, switched by **portals** while the game runs. A switch is no level load and
no reset: Ratchet, his weapons, ammo and bolts, the plugins' state, the session's imported classes and every object that is
not a section's stay as they are. The code: `src/game/enhance/sections.{h,cpp}` (gated by `g_sections_on`: a format 3 build
with a `sections.txt`), the composer's `compose_sections` (`src/setup/level_compose.cpp`).

### The level

level.txt's lines outside any section block are the section **main** (where the ship lands, the level's own collision,
meshes and sky as in format 1). The others are blocks:

```
leave remove                                  # main's objects when Ratchet leaves main: remove (default), freeze, keep
entry from_yard 20 -4 0.5 3.1416              # where portals and section_enter put Ratchet in main
portal east_gate 26 -4 0 31 4 6 to yard:west  # a box in main: Ratchet stepping in goes to the yard's entry west
moby 16:1445 -10 -20 0.5 0                    # a class of another level placed (in main: at the arrival)

section yard remove respawn reset             # its objects when it is left; its own respawn policy (default the session's)
  offset 400 0 0                              # added to the level's offset, for its files and its lines
  collision collision.obj                     # its own collision (none: main's stays)
  meshes meshes.obj                           # its meshes (drawn only while it is the current section)
  sky 16                                      # a game level's sky (none: main's)
  music 16                                    # a game level's tracks, or none (none given: main's)
  fog 120 60 40                               # and background, death_height: as the level's lines, for the section
  lights lights_yard.txt                      # the modern lighting's file while it is current (none: main's)
  entry west -20 4 0.5 0
  checkpoint yard_mid 0 0 0.5 0               # checkpoint_set finds it by name; a death there brings Ratchet back in the yard
  portal back -31 -4 0 -26 4 6 to main:from_yard
  portal to_cave 26 -4 0 31 4 6 to cave:door fade 15
  moby 500 4 -6 0.5                           # the base level's classes, placed by the game's rules (below)
  moby 16:1445 10 14 0.5 3.14                 # classes of other levels (an import of each is added)
end
start yard:west                               # optional: the session starts there once the landing is over
```

The composer writes, per section, `section_<name>/collision.bin` (its own collision block, `build_collision`), `sky.bin`
(the game level's sky block, as a level mod's `sky` line copies it) and `gameplay.bin` (a gameplay file with its placements
only: the records as format 1 copies them from the base level's first placement of each class, their pvar entries and data,
the fix-up lists, the shared pvar data; 0.3 KB for the test's four crates), and `sections.txt` (the sections, their
settings, entries, checkpoints, portals, the classes of other levels placed, each section's range of the meshes in
`host_meshes.mrpc`). The objects' uids continue from main's (the game's per-uid records stay distinct; at most 2047). A
section's objects are spawned in the moby array's room for spawned objects. `compose.log` says per section what it has:

```
[compose] section gameplay: 1808 bytes (4 placements, 4 pvar entries, 1024 pvar bytes), packed 292
[compose] section yard (remove when left, respawn reset): collision 83.7 KB, sky level 16's (228416 bytes), 6 meshes, 4 objects of the base level's classes (uids 7..10), 2 of other levels', 1 entries, 1 checkpoints, 2 portals, death height -10.0
[compose] sections: main and 2 more, 5 portals, 8 objects in the sections (spawned when they are entered: in the room for spawned objects), 1 classes of other levels placed in main
```

### Streaming

At the arrival every section is asked for: a worker thread (`std::async`) reads its files and decompresses its gameplay file;
then, at the start of a game frame, the game's thread **commits** it within a budget (`RAC1_SECTION_BUDGET_MS`, 2 ms): the
session heap's blocks it owns (`section <name>`: freed together by `section_unload`), the copies in chunks of 64 KB, its
collision list relocated (as `level_collision_set` does), its sky relocated (`sub_0023f848`, the current sky's pointer kept),
another level's music table read from that level's header, its objects placed and **frozen** (flag 2 of +0x34: not queued,
neither updated nor drawn; out of the collision grid) until it is entered. A portal to a section not in yet waits for it
(Ratchet keeps playing). The plugin streams with `section_load` / `section_unload` (ABI 10).

```
[section] yard streamed in: asked at frame 730, read and decompressed on the worker in 0.42 ms, committed in 1 frames (at most 0.447 ms a frame, 0.447 ms in all; the budget 2.0 ms), ready at frame 730 (19.9 ms after it was asked); collision 85744 bytes, sky 228416, gameplay 1808, 6 objects placed (frozen until it is entered)
```

### A switch

A portal (Ratchet's position stepping into its box: entries outside the boxes, a box Ratchet is put in waits to be left
first), the plugin's `section_enter`, a death whose checkpoint is in another section, or a loaded save: at the start of the
game frame, once the section is in, after the fade's frames (a black rectangle over the HUD, darker each frame; the same
back after the switch):

1. the leaving section's objects by its policy: **remove** (taken out of play as the game takes a moby out; those already
   killed are remembered and not placed again when it is entered, until a death there), **freeze** (above), **keep**
   (left running). Main's placed objects are main's soft-reset snapshot's: removed ones come back as the arrival placed them.
2. the entering section's state made the game's: the collision's tree and list (`0x173e40`, `0x15f618`), the sky's pointer
   (`0x16051c`), the background (S + 0x23c..) and fog colours (`0x15f444..`, `fog_setup`, `bg_color_set`), the death height
   (`0x15f638`), the music table (the level header's 15 words at +0x148, the track restarted when it changed), the lights
   file (`mrp_user_lights_section`: the render thread reads it again), the meshes drawn (`host_meshes_sections`: the frame's
   capture carries the mask, so the switch shows on the frame the game made it).
3. its objects placed (when the leaving policy removed them) or unfrozen.
4. Ratchet put at the entry (`hero_place`: his state 0, the camera cut behind him); the entry is the current checkpoint.
5. the plugins' `section_leave` and `section_enter` callbacks.

```
[section] frame 1738: main -> yard at entry west (portal east_gate): the switch 0.167 ms (objects left 0.019, state 0.017, objects entered 0.006, Ratchet 0.123); main's objects removed: 7, yard's in play: 6; collision its own, sky level 16's, music level 16's; the session heap 1862 KB
```

### section_spawn_placements and its equivalence test

`section_spawn_placements` (DESIGN 7.4) is `gameplay_load`'s moby part for one gameplay file, into the running session (new
code beside the game's, which is not touched): each placement passing its spawn rule into a free slot of the moby array
(`moby_spawn`'s; a session places an enemy killed "for the visit" again: DECISIONS 4), `moby_init`, its record's fields,
its pvar block into the session heap (packed in placement order as `pvar_move` packs them), the fix-ups (the pvar words
naming a placement: its slot; the offsets into its own block; the pointers into the shared pvar data, one block a section).
The groups are not placed (the mod's sections have none: stage 8's hook).

The test (`RAC1_SECTION_EQUIV=1 RAC1_SESSION_MEM=1 RAC1_LEVEL=<n>`): at the end of `gameplay_load`'s pvar fix-ups the level's
gameplay file is read again from the disc, the native mobys taken out of the grid and cleared, and the file placed again by
`section_spawn_placements` into the same slots (in order from the array's start, the moby queries' stamp counter as the
load had it): every moby record must be the native load's byte for byte, every pvar block too with its own offsets and its
shared-data pointers rebased, and the placement -> slot map the same. On all 19 levels:

| Level | Mobys | Pvar words (rebased) | Result |
|---|---|---|---|
| 0 Veldin | 296 | 12 668 (359) | ok |
| 1 Novalis | 929 | 39 440 (825) | ok |
| 2 Aridia | 742 | 39 508 (977) | ok |
| 3 Kerwan | 965 | 62 284 (1 274) | ok |
| 4 Eudora | 494 | 31 764 (624) | ok |
| 5 Rilgar | 1 405 | 86 252 (2 026) | ok |
| 6 Blarg | 719 | 54 544 (1 213) | ok |
| 7 Umbris | 648 | 29 576 (864) | ok |
| 8 Batalia | 783 | 33 692 (645) | ok |
| 9 Gaspar | 862 | 38 768 (1 068) | ok |
| 10 Orxon | 828 | 79 132 (1 364) | ok |
| 11 Pokitaru | 720 | 67 484 (1 641) | ok |
| 12 Hoven | 678 | 79 084 (1 101) | ok |
| 13 Gemlik | 943 | 47 400 (1 293) | ok |
| 14 Oltanis | 542 | 40 252 (647) | ok |
| 15 Quartu | 850 | 46 180 (1 253) | ok |
| 16 Kalebo III | 1 534 | 54 568 (1 408) | ok |
| 17 Drek's fleet | 728 | 41 780 (887) | ok |
| 18 Veldin (finale) | 991 | 52 848 (1 516) | ok |

(The first try differed on four levels in +0x9c only: the moby queries' stamp, a counter at `0x173e50` the ground probes
count up; the test now starts it where the load did.) The test covers the placements' rules and fields; a session's own
placing differs only in the slots (free ones of the spawned room) and the pvars' address.

### Deaths, saves, leaving

- **A death** in a section: `session_respawn` first makes the checkpoint's section the current one (at once, no fade), then
  puts Ratchet there as before, then resets the current section's objects by its policy (its own `respawn`, else the
  session's): `reset` places again its killed ones (and those killed before it was last left), the game's killed-for-good
  ones not; in main, as stage 5 (its snapshot) plus its classes of other levels.
- **Checkpoints** belong to a section: level.txt's in a block, an entry Ratchet came in by, `checkpoint_at` / the game's
  own in the current one.
- **Saves**: the `.pc` item's version 2 lists every section (its name; the checkpoint in the checkpoint's section; its
  objects gone); a load streams the checkpoint's section in and switches to it once the landing is over, before Ratchet
  is put at the checkpoint. Stage 5's version 1 items are read as main's.
- **Leaving** (the ship, a save of another level): the sections dropped with the session (their heap blocks freed, all
  meshes drawn again, the lights file the level's).

### Testing

The stage's test mod is a copy of the example level (`s6-sections`: main the plateau, `yard` the plateau again 400 units
east with Kalebo III's sky and music, four crates and two of Kalebo III's robots, `cave` a room of our own 400 units north
with Veldin's sky and no music, a crate and a robot; five portals) with plugin test hooks (`RAC1_MOD_TEST=s6_tour`,
`s6_switch50`, `s6_walk`, `s6_save`, `s6_load`, `s6_ship`):

| Test | Result |
|---|---|
| `s6_walk` (`RAC1_SIM_INPUT=forward`) | Ratchet walks from main into the east portal, across the yard on its own collision (44 units at z 20, 400 units from main's ground), into its east portal, and in the cave stops against its pillar (main's collision has nothing there) |
| `s6_tour` | main -> yard (plugin) -> a death in the yard (back at its entry, in the yard) -> cave with a 15-frame fade -> a death in the cave -> main through the cave's portal (20-frame fade) -> a death in main -> yard by main's portal -> a crate taken out -> main by the yard's portal -> yard (the crate still out) -> a death (the crate back) -> the cave unloaded and entered again (streamed while playing: 3.7 ms on the worker, 0.28 ms committed, the switch 5 frames after the ask) |
| `s6_switch50` | 50 switches main <-> yard, 25 by portals, 25 by `section_enter`: each 0.06 to 1.07 ms (median 0.33), the session heap the same after them |
| Pictures (`RAC1_SECTION_DRAW_ALL` to compare) | each section's sky and meshes; with the mask off, the cave's walls show at main's horizon, with it they do not |
| A save in the yard (`s6_save`, `RAC1_SESSION_SAVE_AT=2100:2`), loaded on Kerwan (`RAC1_GOTO=3@60 RAC1_SESSION_LOAD_AT=1200:2`) | the item: 3 sections, checkpoint `yard_mid` in the yard, its crate gone; after the load's landing the yard streamed in, the switch, Ratchet at `yard_mid`, the crate still gone |
| The ship (`s6_ship`: to the yard and back by its portal; `RAC1_UNLOCK_PLANETS=3 RAC1_LEVEL_MAP_AT=2200` and a pad script choosing Kerwan) | the session left at Kerwan's header, its sections dropped, Kerwan played; choosing the level again enters it anew |
| The original; formats 2 and 3 | the reference pictures and RAMHASH unchanged; the arena's format 2 regression and stage 5's format 3 runs (`s5-arena` 50 deaths, `s5-plateau`) with the same RAMHASH as before |

**Another level's sky** (a section's or the level's `sky` line) is drawn by that level's own sky variant, not the base's
(`sky_render`, `src/game/ovl/sky_levels.cpp`; the level from build.json's `sky` and sections.txt): each variant is made for its
own sky. Before, the base's ran on it: Veldin's makes 256 stars in the sky's sprite list (the header's +0x1c, room for +0xa
entries), so Umbris's (7), Gemlik's (13), Rilgar's (5) and Quartu's (15), with room for 128, had their first shell's header
overwritten by the 129th star (a cluster count of about 0x08000000: the game ended in the sky's draw), and the skies
with no list had 256 stars written at EE address 0.

## Rails and set pieces (stage 7)

DESIGN stage 7 and the study's 5.1 to 5.5: grind rails from level.txt, the hero code's landing gate, items for the visit,
Magneboots surfaces, and the set-piece kit's services (a fade, a camera service, hazards, moving platforms, classes with a
collision mesh of their own). The code: the composer's rails and collision meshes (`src/setup/level_compose.cpp`:
`rails_resolve`, `grind_block`, `rail_strip`, `rail_model`, `class_collision`), `src/game/enhance/setpieces.{h,cpp}` (rails at
run time, items, the fade, the camera, the hazards), `mod_code.cpp` (the collision meshes, `object_follow_path`, the SDK's
table), `sections.cpp` (a section's rail table).

### Rails

- **The composer** reads `rail <name> [loop] [mesh [<material>]] [nostrip] <points>` and `rails <file.obj>` (each OBJ object's
  line elements one rail: its edges chained, a closed chain a loop) in main and in section blocks, moves them by the
  offsets, puts a point every 1.5 units, and writes the gameplay file's block +0x74 as the game's levels have it
  (`level_load.cpp`'s grind lines: n, the points' offset and size; n records of 0x20, the bounding sphere and the loop flag;
  n path offsets; the paths: a count, then x y z and the length to the next point). Main's go into `gameplay.bin` (the
  level's load lays them out: no run-time code), a section's into its own `gameplay.bin` (a gameplay file of rails only when
  it places no objects). Under each rail a collision strip (0.5 wide, 0.02 under the line) goes into the collision it rides
  over (the section's own, else main's): `grind_rail_update` lets go of a rail when the ground under Ratchet is more than
  0.87 away (+0x2dc). `mesh` adds a model (a 0.2 x 0.2 bar whose top is the line) to the host meshes, in the section's range.
  level.sdk lists them (`rail <name> <section> <index> <loop> <points>`) for the SDK.
- **Sections**: at a section's commit its rail table is made from its gameplay file's copy in the session heap (the records
  in a block the section owns, their paths pointing into the copy), and `apply_state` makes it the game's (0x15f70c /
  0x15f710; main's as the arrival found it; none for a section without rails), so the hero's rail search reads the current
  section's only.
- **The landing gate**: `hero_rail_land_check` (0x20cf58) takes a rail on Kalebo III (16), Oltanis (14) and Blarg's station
  (6) only within 12 (14) units of their own rail spots; on a level mod with rails of its own (`setpieces_rails_anywhere`:
  its level.sdk lists rails, the level a level mod's) the gate is skipped. A level mod without composer rails (the night
  arena's plugin-made table) keeps it, as before.
- **At run time** (`setpieces_frame`, once a frame before the plugins' frame callbacks): the rail Ratchet is on (states
  0x28..0x2b and 0x42; his path +0x560 against the current table's records), the `rail` callback's ENTER / END / LEAVE;
  `rail_enable` takes a rail out of the search by its record's reach (-1: `find_rail_v` skips it), put back at each switch to
  its section; `rail_speed` scales +0x574 (the next frame's step); `hero_rail_attach` puts Ratchet standing on the rail's
  line facing along it (`hero_place`): the next hero update's landing check latches it and state 0 takes it (0x28). The
  game's grind always starts along the points (+0x578 = 1): riding back, the attach sets -1 once he is on it.

- **Grind or grab? One table.** A mod rail can be taken from below with the wrench, as Kerwan's overhead rails are (a jump under
  it: Ratchet hangs from it, state 0x74, along its points). The game has no per-rail information to tell the two: a rail record
  is the bounding sphere (file +0x0 .. +0xc), the loop flag (file +0x10) and three words that are zero in the original's
  gameplay files of every level that has rails (the table the load makes: the sphere, the path +0x10, the loop flag +0x14,
  zeros +0x18 and +0x1c); Kerwan's three rails (paths of two points) are records of the same layout as Blarg's, Batalia's,
  Oltanis's, Quartu's, Kalebo III's and level 18's. `find_rail_v` (0x20cd08) scans that one table for both: `hero_rail_land_check` (0x20cf58, the feet, with the
  grind boots) and `hero_rail_grab_check` (0x20d330, the hand point, a jump's state 2 or 4); what differs is the hero code (the
  generic code calls both, every Novalis-family build only its own: Kerwan's the grab, none of them the landing) and where
  Ratchet's feet and hands are. There is no `rail` option for it: a rail placed over where Ratchet jumps from below is grabbed;
  place it out of the hand's reach (above the jump's) or run `rail_enable` (ABI 11) off while he is under it.

### Items, Magneboots

- `items` (level.sdk) and `item_visit`: the owned byte 0x13d4c0 + item set at the level's start when not owned, cleared
  when a level loads after it; `save_blocks_write` (the card's save image) is written with them cleared
  (`setpieces_save_begin` / `_end`). Proof: a save in the session, loaded from Kerwan: back in the session the two items are
  given again (they were not in the save); `RAC1_GOTO=s7-kalebo@60,3@1700`: `2 items of the visit taken back` at Kerwan's
  load.
- **Magneboots surfaces** need nothing new: `usemtl type2` faces set the hero's +0x12e7 (`hero_surface_update`) and the
  Magneboots (0x13d4dc) do the rest. Tested on a quarter-pipe of our own (a floor arc, a wall 10 high, an arc, a ceiling at
  30): Ratchet walks up the wall and onto the ceiling (state 0x3f) on Veldin's code (format 3) and on Kalebo III's (format
  2); on Kalebo III's his model was drawn wrong in the magnetic walk (its hero class lacks that animation: given by stage 9,
  [Hero contexts](#hero-contexts-stage-9)).

### The set-piece kit

- **Fade**: `fade(out, hold, in, rgb)`, drawn over the HUD (`gs_flat_rect`) as the section switch's fade; `fade_active`.
- **Camera**: `camera_set` writes the view after the game's camera update (`mod_code_camera`): the position 0x166dc0, the
  rows 0x166fd0.. from the Euler angles (`euler_to_mat4_vu`) and the angles 0x166dd0 (the game's pitch is down positive;
  the SDK's up positive); modes fixed, look-at (a point or a moby), along a path (looking at a point, a moby or ahead), follow
  (behind a moby); a blend from the view at the call; `camera_release` blends back to the game's live camera.
- **Hazards**: boxes or spheres, on a timer, optionally moving with an object, tested against Ratchet's middle each frame:
  the game's own hurt (`hero_damage`: his knock-back and grace time).
- **Moving platforms**: `object_follow_path` moves a level object in the objects' pass (before its class's update). Ratchet
  rides it with the game's own ride (`hero_platform_ride` 0x2134c0): the object's moby is given flag 0x20 and pvars whose
  word +8 names a platform block (`moby_pvar_word2`), into which each frame's move is written (+0x10; the turn +0x00 none):
  the game's platforms do the same.
- **Collision meshes**: `class ... collision <coll.obj>`: the composer writes the game's moby collision format (as the
  bolt crate's box, read at run time: a 0x10-byte header with no joints and no primitives, int16 vertices at 1/1024 of a unit
  of the moby's scale, triangles clockwise seen from outside with the flag byte 0x7f) into `class_<name>.col`; the game
  copies it into the mods' block (8 KB for a level's) and sets the carrier's +0x94 to it; since the carrier crate's
  bounding sphere is a 1-unit box's, the object's sphere and its place in the moby grid are made from the mesh's (its
  transform ours: flag 4).

### Testing

Test mods (`openengine/s7/make_s7.py`): `s7-kalebo` (format 2 on Kalebo III) and `s7-sections` (format 3 on Veldin, a yard
section 400 units east with a rail of its own), the example plateau with two straight rails 3 units apart, a ring (a loop)
and a slope from `rails.obj`, the magnetic quarter-pipe, a deck class (a U shape: a floor and two walls, its collision mesh)
riding a path, a post (a box); plugin hooks `RAC1_MOD_TEST=s7_*`:

| Test | Result |
|---|---|
| `s7_rail` (Kalebo III) | `hero_rail_attach` at 2 units: ENTER the next frame, 0.2 units a frame after the start, END at 47.1 of 48, LEAVE as he flies off the end (far from Kalebo III's rail spots: the gate skipped) |
| `s7_back` | attached at 46 riding back: rides to its first point (END at 0.9, LEAVE) |
| `s7_speed` | `rail_speed` 2: the end 138 frames after the start instead of 250 |
| `s7_side` | on `low`, a jump with the stick left: across to `high` 3 units beside (LEAVE low, ENTER high at the jump's start), its end reached |
| `s7_land`, `s7_disable` | dropped 2.5 over the ring: lands on it (ENTER); taken out first: falls through, put back: lands |
| `s7_magnet` (both bases) | forward from the floor: up the arc (`+0x12e7` 1, state 0x3f), up the wall to z 20, onto the ceiling at z 29.8 |
| `s7_deck` | Ratchet placed on the deck (moving 3 units a second back and forth, rising 4): rides it across both ends of its path; walking north he stops at its wall (y 1.23 from its middle: the mesh's wall at 1.7) |
| `s7_spin` | the deck stopped and turned 0.02 a frame by `object_place`: Ratchet 1.0 off its middle goes round with it, full turns |
| `s7_hazard` | a box on 60 off 120: hurt when it comes on (4 -> 3), then the game's grace time |
| `s7_cam` | fixed, looking at the post (centred), along `cam_path` looking at Ratchet, following him, released over 30 frames: pictures |
| `s7_fade` | out 20, held 30 (the picture black), in 20 |
| `s7_items`, a save | given at the arrival, `item_visit` off / on, the save without them, taken back at another level |
| `s7_sec` (Veldin, sections) | to the yard: its rail attachable, main's not (not the current table); back to main: main's attachable, the yard's not; ENTER / END / LEAVE on both; Veldin's hero grinds as Kalebo III's |
| The original; formats 2 and 3 | the reference pictures and RAMHASH unchanged; the arena's format 2 regression and stage 5's format 3 runs with the same RAMHASH; the composer's output for the four regression mods byte for byte the same as before |

## Game worlds as sections (stage 8)

DESIGN stage 8: a section's scenery can be a game level's world. The code: `src/game/enhance/section_world.{h,cpp}` (the
world's files, its build in the session heap, the engine's world state), `sections.cpp` (a world section's streaming, its
switch, its objects and groups), `leveldata.cpp` (`ld_engine_take` / `ld_engine_restore`, `ld_group_put`), the composer's
`compose_world` (`src/setup/level_compose.cpp`), the modern world renderer's level (`mrp_world_renderer.cpp`:
`world_renderer_level_get`, `mrp_world_prefetch`, the kept world levels).

### What the design said, and what was done

DESIGN.md's stage 8 exported a level's world "as host meshes" (the composer converting tfrags, ties and shrubs into the level
mod's own mesh format). That was not done: the world is the game's own, in EE memory, registered and lit by the game's own
functions, culled by its own passes (their LODs, the zone grid, the frustum), and the modern renderer decodes the passes'
chains as on the planet itself (its index already holds any world's packets by content). What it draws is the planet's world
pixel for pixel where the camera and the frame are the same (below), and nothing is converted or approximated.

### The level

```
section kalebo remove respawn reset
  world kalebo                     # level 16's world (a name or a number)
  objects kalebo.robot 52 638 500  # its placed objects of these classes (a name, its class id, `all`); none by default
  box 130 110 110 200 275 140      # only its objects inside the box (optional)
  entry arrive 146.43 133.23 128.3 1.46     # its level's coordinates: no offset in a world's section
  portal back 140 136 125 144 140 132 to main:from_kalebo fade 10
end
```

The composer (`compose_world`) reads the level's gameplay file from the disc, chooses the placements (class, box), gives each
a **session uid** after main's and the other sections' (`wuid <section> <its uid there> <the session's>` in `sections.txt`:
the game's per-uid records stay distinct and below 2048), and adds an import of each chosen class with the closure (an
`imports.txt` line `world-of-<section>`; a class the base level has is the base's; a class with no data in the pool, code
alone, is not placed and said so). A world's section has no `meshes`, `rail` lines, base-level `moby` lines or `offset`; its
own `collision`, `sky`, `music`, `fog`, `background`, `death_height` and `lights` lines replace the world's. Its lines are not
moved by the level's offset.

### Streaming and the build

The worker reads the level's `data.wad` (its index, its asset data decompressed: 21 MB) and its gameplay file
(`gameplay_ntsc.bin`, `gameplay_pal.bin` on a PAL disc) and keeps of them the world's blocks, each up to the next block the
index names: the tfrags (2.6 MB on Kerwan), the zone grid (1.4 MB), the texels of the world textures (4.5 MB: the passes
upload them to the GS each frame, from the textures' tables), every tie class and shrub class, the collision and the sky. The
game's thread then commits, within the budget a frame (`RAC1_SECTION_BUDGET_MS`, 2 ms), in steps of `world_build_step`:

1. the blocks copied into one heap block of the section (64 KB a step), its index beside them;
2. the registrations, as `level_setup_body` makes them: the three world textures' tables (the texel base the section's), the
   tfrags (`moby_lod_set`: their LOD distances and texture words), the zone grid, every tie class (`tie_class_register`) and
   shrub class (`shrub_class_register`);
3. the tie instances (0x20-byte bounds, 0x1c0-byte data: gameplay_load's code, 256 a step), then their static lighting
   (`world_tie_lighting`, 24 a step), the shrub instances and their lighting (`world_shrub_lighting`), the tfrag entries'
   lighting (`world_tfrag_lighting`, 96 a step), all with the level's own lights (its directional lights and light grid);
4. the LOD / visibility masks of its tfrags and ties (the gameplay file's +0x8c lists) and the zone mode.

Each step puts the section's world state into the engine's (the words and tables below), runs, takes it back and puts the
current section's back. The result is the section's **world state**: 25 words (the tfrag entries, their count, the LOD
distances, the zone grid and its mode, the tie and shrub classes' counts and instance tables, the world textures' counts) and
the tables (the LOD bands 0x1c28f0, the tfrag textures' table 0x1c4a00, the tie classes' headers, ids, id map and textures'
table 0x1c5800..0x1c6b00, their radii and palettes, the shrubs' 0x1bc030..0x1bc9b0, radii, palettes), 11 KB, and its **lights**
(the light grid and the directional lights, 0x17ecc0..0x1803c0 and 0x1804c0..0x180640). The level's context and registry
(its level data) are made at the session's arrival, not within a frame (their files are read: 10 to 20 ms).

```
[section] kerwan streamed in: asked at frame 974, read and decompressed on the worker in 215.99 ms, committed in 60 frames (at most 3.064 ms a frame, 133.129 ms in all; the budget 2.0 ms), ready at frame 1033 (498.7 ms after it was asked); collision 3628992 bytes, sky 150912, gameplay 2395648, 9 objects placed (frozen until it is entered), 3 rails
[section] kerwan: level 3's world: 160 blocks, 10.7 MB copied of its 21.4 MB of asset data; 1006 tfrags, 127 tie classes with 5105 instances, 30 shrub classes with 985 instances, zone grid its own; built on the game's thread in 69.47 ms over the commit's frames; in the session heap 19.2 MB (world 13.3, collision 3.4, sky 0.14, gameplay 2.2); the session heap 20303 KB
[section] kerwan: the world's steps' longest calls (ms): copy 0.08, registrations 0.85, tie instances 0.21, their lighting 0.94, shrub instances 0.08, their lighting 0.58, tfrag lighting 0.95, masks 0.09
```

Memory: Kerwan's world 19.2 MB in the session heap (its blocks 13.3, collision 3.4, sky 0.14, gameplay file 2.2), Kalebo III's
18.0 MB (13.3, 2.3, 0.21, 2.1): a whole world fits (no region of a world was needed); both with the session's imports: 40.1
MB of the heap's 192. On the host, the worker's copy of the asset data (21 MB) is dropped once the section is committed.

### The switch

Into a world's section (`apply_state`): its world state and lights the engine's, its fog's words (0x15f448..0x15f454), its
level's registry's objects the engine's (`ld_engine_take`: its paths, cameras, volumes, rails and groups are what the base's
engine code reads, the camera zones, the volumes; the imported classes' own swap `ld_enter` nests inside), its collision, sky,
background, fog, death height, music and rails (its gameplay file's grind lines, the section's rail table), and the modern
renderer's level its level's (`world_renderer_level`). The camera system is made again from the cameras now the engine's
(`camera_init` / `camera_update`, as a level start makes it: the planet's start camera). Out of it: the session's own,
taken at the arrival. Ratchet, his weapons and the session's objects are untouched.

```
[section] frame 1798: kerwan -> kalebo at entry arrive (portal to_kalebo): the switch 0.114 ms (objects left 0.010, state 0.009, objects entered 0.010, Ratchet 0.084); kerwan's objects removed: 9, kalebo's in play: 53; collision its own, sky level 16's, music level 16's; the session heap 40109 KB
```

The modern renderer: when the current section's level changes, its world level is loaded (`load_level`); a world's asset and
textures are read on a worker when its section streams in (`mrp_world_prefetch`), so the first entry costs the GPU uploads and
the classes' and tfrags' tables from EE memory (43 to 73 ms in the tests, under the fade), and a world level is kept when its
section is left (at most two, with their GPU textures and buffers) and taken back in 0 ms while its section's tables are where
they were. The session's own level (its host meshes) is loaded again when Ratchet comes back (0 to 12 ms). While a world's
section is current the level mod's own meshes (main's, the other sections') are not drawn; what the world's level gets of
them (`world_host_meshes`, `mrp_world_renderer.cpp`, at its load) is the session's host mesh classes it shows: its sections'
own instances (the mod's rail models there, `sections.txt`'s `meshes` range of a `world` section of that level) and the
models of level.sdk's classes, added after the planet's tie classes (`World::host_class_map`: a session class to its tie
there, for the objects' instances), their textures after its tie textures (CLAMP_1's MINV moved past them), then bound as
the session's level binds its own: the scene draws them as it draws the session's (GL and Vulkan alike: one scene). A world
level kept and taken back keeps them. The objects there are those of that section: its `object` lines (level.sdk's
`sobject` lines), and those spawned, placed or shown in it (`mod_code.cpp`'s `Obj::section`; `sdk_publish` draws them and
takes the others' collision away, `Obj::away`, giving it back when they are there again). The composer puts a world section's rail models after every other host
mesh (the classes' models included), so adding one moves no index lights.txt's `material tie` lines name (composer 7).

### Objects

A world's placements are placed by `section_spawn_placements` from the level's own gameplay file, with three options: the
placements the composer chose, each with its session uid (the spawn rules and +0xb2 the session's uid; the rules' level state
is the session's: a first visit), each class the session's id for it (an import's, else the base's own), and each moby's zone
mask (+0x36) from the level's +0x8c list by its uid there. Then the level's **moby groups** (+0x48: lists of placement
indices) are made in the level's registry from the objects in play (`ld_group_put`, the lists the section's objects' blocks;
emptied when they go), so the objects that update as a group (+0x21) do. Removal, freezing, deaths and saves are stage 6's.

### Testing

The test mod `s8-worlds` (`openengine/s8/mods`): main the example plateau, `kerwan` (level 3's world, its crates near the
start) and `kalebo` (level 16's world, its robots 1445, the pods 52, the turrets 638 and its crates in a box), portals both
ways between all three; plugin hooks `RAC1_MOD_TEST=s8_*`. A variant `s8-full` takes `objects all` in a box of each (91
classes imported, the class table 218 of 224).

| Test | Result |
|---|---|
| `s8_tour` | main -> kerwan (plugin) -> kalebo (kerwan's portal) -> main (kalebo's portal) -> kalebo (main's portal) -> kerwan (kalebo's portal) -> main (kerwan's portal) -> kerwan (main's portal): each world its own, Ratchet on its collision at its entry |
| `s8_switch` | 60 switches main -> kerwan -> kalebo -> main..., half by portals: 0.108 to 0.520 ms (median 0.199); the renderer's world level taken back 38 times in 0 ms; the session heap the same after them (40106 to 40131 KB: the objects' pvars) |
| Pictures (GL, Vulkan), the same place and camera as the native level (its start camera) | the world alone (`RAC1_RENDER_MOBY=0 RAC1_RENDER_FX=0`): Kerwan 97.3 % of the pixels within 4 of the native picture's, Kalebo III 85.6 % (its clouds scroll with time: the rest is the sky and edges' subpixels); the full pictures differ by the planet's objects the section does not place |
| Collision (`RAC1_COLL_PROBE`: the game's floor probe on a 2-unit grid) | Kalebo III: 8096 of 8181 points the same height as on Kalebo III; Kerwan 4339 of 4356; the others where the native level has objects the section does not place (the ship, crates, a bridge) |
| `s8_walk` (forward from Kerwan's start) | the same path as on Kerwan: over the bridge, down the steps, stopped by the wall at 264.18 151.49 50.00 (the native run's stop: 264.18 151.49 50.00) |
| `s8_robots` (`RAC1_SIM_INPUT=attack`) | Kalebo III's six robots walk to Ratchet and hit him (4 -> 3), die to the wrench, drop bolts (20); a death there: back at the checkpoint in kalebo, the six robots back, fighting again |
| `s8_rail` | dropped onto Kalebo III's own rail 4: carried along it (the section's rail table: its gameplay file's) |
| `s8_die` | a death in each world: back at its checkpoint, in it (two Kerwan crates missing afterwards were not the respawn's: they sit on Kerwan's lift 1210, which the test's `objects` did not choose, and fell through at the entry; with 1210 chosen all nine are there before and after the death: stage 9's `s9_die`) |
| A save in kalebo (`s8_save`, a crate taken out, `RAC1_SESSION_SAVE_AT=1800:2`), loaded on Kerwan (`RAC1_GOTO=3@60 RAC1_SESSION_LOAD_AT=1200:2`) | the item: checkpoint `arrive` in kalebo; after the load's landing kalebo streamed in, the switch, Ratchet at its checkpoint, the crate still out |
| `s8_unload` | kerwan left, unloaded (its 19 MB given back), entered again: streamed in again while playing, the renderer's kept level taken back (the tables at the same addresses) |
| The original; formats 2 and 3 | the reference pictures and RAMHASH unchanged; `RAC1_SECTION_EQUIV` all 19 levels ok; the arena's format 2 regression and stage 5's format 3 runs with the same RAMHASH; the composer's output for the regression mods byte for byte the same |

## Hero contexts (stage 9)

DESIGN stage 9 (and 5.8): Ratchet run by another level's own hero code in a session, and the game's own hero modes (Clank,
Giant Clank, the hoverboard) entered from a section or a plugin. The code: `src/game/enhance/hero_modes.{h,cpp}` (the hero
context, the modes, the hero's sequences), `variants.h` (`on_hero_level`), `context.cpp` (`own_code_tables`), `camera.cpp`
(`g_camera_types_table`), `sections.cpp` (a section's `hero` line), the composer's `hero` lines (`level_compose.cpp`), the SDK's
ABI 12 (`mod_code.cpp`).

### What the design said, and what was done

DESIGN 5.8 set moby 0's update function to a tag of A's hero update. That was not needed: the hero's update is Veldin's port
on every level, and the ports of the hero controller already dispatch, line by line, to a level's transliterated build
(`if (on_kalebo()) { kal_call(kal_hero_set_state_u ...) }`) while that level is the translation target. A **hero context** is
a second target for those lines only: `g_hero_level` (variants.h), which `on_kalebo()`, `on_quartu()`, `on_gemlik()`,
`on_orxon()` and `on_rilgar()` also accept (`on_hero_level`). Their sites are the hero controller's and the camera system's
(camera types, camera hooks); the sound emitters' and the cutscene extras' lines ask `on_level` (the overlay's only). The
build's own addresses go through the level's context as any of its classes' do. -1 always on the original's levels.

Three things the build needed beyond the dispatch:

1. **Its jump tables.** A level's own build of an engine function (a variant) keeps its `switch` tables in data blocks its map
   pairs with Veldin's (the same place in the overlay): through the context its code read Veldin's table, whose labels its
   `case`s never match. Kalebo III's `hero_set_state` found no case for the board state 0x6b: Ratchet stood on the board
   without riding it. `own_code_tables` (context.cpp): in a context's engine blocks, a run of two or more words that are
   labels of the level's own code is such a table: the level sees its own copy (a view onto its image, the words made tags;
   a paired function's start in it Veldin's). Kalebo III: 87 tables, 1245 words; Kerwan 87 (1369), Umbris 82 (1353), Gemlik
   87 (1365). The imported classes' code never read those tables (their own functions' tables are their own data already):
   every regression of stages 5 to 8 has the same RAMHASH with them.
2. **Its camera types.** Kalebo III's race camera is its own camera type 8 (3 and 8 are its own), Quartu's Giant Clank's 0x16;
   the camera system finds a type's functions in the overlay's type table (0x1ea880), and Veldin's has no rows 3, 8 or 0x16.
   The hero context copies the session's table into the session heap with the level's own rows (from its context image: the
   functions tags) put in place of the same id's or added before the end row, and the camera system reads the copy
   (`g_camera_types_table`); the cameras are made again (`camera_init`, as a level start) since their rows (+0x8c) are indices
   into the table. Back to the overlay's table when the context goes.
3. **Its hero's sequences.** The hero's animations are his class's 256 slots, filled from the level's core at its load
   (dev_hoverboard.cpp found it). Veldin's core has every one; another base's lacks some (Kalebo III's: 91, the magnetic
   walk). On a level mod's level, when the hero is about to play a sequence his class has no slot for (`hero_anim_set`'s two
   ports ask `hero_seq_need`), it is copied from a donor level's core (the hero context's level, else Veldin's), read from the
   disc on a worker, into `host_layout::HERO_SEQS` (0x02000000, 3 MB above the game's 32 MB), its frame table and trigger
   block made addresses as the game's load does. In a format 1 / 2 level mod only the magnetic walk's (Ratchet on a
   Magneboots surface, +0x12e7): its core is read the first time he is on one.
   **His sounds** likewise: every level's hero class has the same 64 sound definitions, but each level's bank holds only the
   sounds its own hero uses, the others' sound -1 (nothing plays): Kalebo III's bank lacks 28 (among them the burn in the
   lava, definition 32, and the sinking, 9), Veldin's only 2 no level's bank has. On a level mod's level (any format), once
   it is in play, the definitions its base's bank lacks are pointed at Veldin's bank (the pool's `sounds/level00.bnk`, loaded
   once as a host bank and kept for the run, 1.4 MB of host memory) and its sounds (the pool's row of Veldin's class 0):
   `[hero] 26 of the hero's 64 sounds are not in level 16's bank: Veldin's`. Without a pool they stay silent.

**Giant Clank's energy.** The bar (the player block's +0x1630, 0 to 200, the element 0xffff on Quartu's HUD) is set to 200 by the
game's `hero_mode_enter` for the form 2; hero_hurt takes from it, and with 0 left the states choose 0x62 (he falls: Ratchet is
dead and the soft reset follows). On a level mod whose base is Kalebo III the hero controller's dispatch takes Kalebo III's own
`hero_mode_enter` (`on_kalebo()` is true on its overlay whatever the hero context), which has no form 2: the bar stayed 0 and a
plugin's Giant Clank fell after ten to fourteen seconds (`go 1440: state 0x62 energy 0`). `hero_mode_set` now sets the energy
to 200 itself after entering the form 2, and the SDK's `hero_energy` (ABI 18) reads and refills it. Checked: 2000 frames in
Quartu's arena (as a plugin's Giant Clank in `world quartu`) with the energy at 200 throughout; set to 50 it stays 50, back to
200, and 0 makes him fall (state 0x62, the soft reset, Ratchet back at the checkpoint).

The modes are the game's own: `hero_mode_enter(form, state, object)` (Clank, the form 1: Orxon's class 87, state 0x43; Giant
Clank, the form 2: Quartu's class 419, state 0x5a, as `level_setup` starts a level in a vehicle) and `hero_mode_exit` (Ratchet's
moby back at the object's place, state 0, Clank's health swapped back); the board is the board's own mount (Kalebo III's or
Rilgar's class 439: the board given to the hero +0x86c, the board state 0x6b, the board in its riding state, as its update does
when he walks up to it). Veldin's hero code has Clank's and Giant Clank's states (Orxon and Gemlik run on it natively); the
board's riding is Kalebo III's and Rilgar's own.

Not every planet's hero code differs from Veldin's: Orxon and Gemlik run Veldin's (their contexts give only their camera
code), and Veldin's has Clank's and Giant Clank's states. **The generic hero code** (`hero generic`, `HERO_GENERIC`) is the other
way round: on a level mod whose base has a build of its own (Kalebo III, Quartu, Rilgar), the base's own hero lines are turned
off (`on_hero_level` false for every level) while its camera code stays (`on_camera_level`, the camera system's sites): Veldin's
ports run the hero on the base's overlay, translated as on any level whose hero is the generic one (Gaspar, Umbris). Any
format: the arena (format 2 on Kalebo III) gets the game's lava (0x68 sinking, Cross 0x69 out, lost when left in it: the
level loaded again) with its animation (113, from Veldin's core: the whole hero set is read at the load when the generic code
is asked), its waves and rail course the same as on Kalebo III's code (the same challenge lines and results).

### The level

```
section race remove respawn reset
  world kalebo
  hero kalebo                      # level 16's hero code and camera code while Ratchet is in the section
  objects 439                      # its race board: he gets on it when he walks up to it
  entry board 145.3 90.3 83.7 -1.5708
end
section giant remove respawn reset
  world quartu
  hero quartu giantclank           # Quartu's code; Giant Clank (15:419 imported, spawned at his entry)
  entry start 130.5 222.4 26 0
end
```

`hero <level> [clank|giantclank|board]` in main or a section (format 3; main's `hero` line with a name is this line, with numbers
Ratchet's start as before; `hero generic` in any format): the composer checks the level (5, 10, 13, 15, 16),
imports the mode's class (10:87, 15:419, `<level>:439`) and writes `hero <section> <level> <mode>` into `sections.txt`. The
switch (`apply_state`) sets the entered section's hero context (its line's, or the base's); the leaving section's mode is left
first (`hero_mode_leave`), the entered one's object spawned where Ratchet stands and the mode entered at the first frame of
play after the switch (after its fade). At the arrival the hero lines' contexts are made with the worlds' (not within a frame).
A death in a mode: the mode left before the soft reset makes the hero again, entered again (a new object) once he stands at
the checkpoint.

### Testing

Test mod `s9-heroes` (`openengine/s9/mods`): stage 8's main, kerwan and kalebo, with `world umbris`, `world gemlik` (`hero
gemlik`, its ship 69), `race` (Kalebo III's world, `hero kalebo`, its board), `clank` (Orxon's world, `hero orxon clank`),
`giant` (Quartu's, `hero quartu giantclank`); plugin hooks `RAC1_MOD_TEST=s9_*`; `RAC1_HERO_STATE=<n>` prints the hero's
state, sub-state, form, place and animation every n frames on any level (the native runs' traces).

| Test | Result |
|---|---|
| `s9_board` (`race`: `hero kalebo`, the board 16:439, stick forward, a jump) | the board's own code mounts him at once (state 0x6b, sub 0x16, the board +0x86c), Kalebo III's code rides it: along the track at 0.24 units a frame, jumps (85), a wipe-out on a wall (0x6d, 97), back on the track; the race's HUD (lap 1/3, the time) and its camera (Kalebo III's type 8) following him; out of the section: Veldin's code, Ratchet on foot |
| `s9_boardp`, `s9_free`, `s9_freemain` (the plugin: `hero_build(16)`, `import_spawn` 16:439, `hero_mode(BOARD)`) | a board ridden anywhere: Kalebo III's courtyards (off an edge: back where he got on), the plateau of main; `hero_mode(NONE)` gets him off where he is |
| `s9_clank` (`clank`: `hero orxon clank`) | Clank (10:87) spawned at the entry and made the hero (form 1, state 0x43): walks (0x44), jumps; back in main as Ratchet, health 4 |
| `s9_clankdie` | Clank walked into Orxon's toxic water: his death (0x7d), the soft reset (the mode left first), Clank again at the checkpoint |
| `s9_giant` (`giant`: `hero quartu giantclank`) | Giant Clank (15:419, form 2, 0x5a): walks (0x5b), punches (0x60) on Quartu's code and camera; back in main as Ratchet |
| `s9_modes` (main, Veldin's code) | Clank from the plugin (`import_spawn` 10:87, `hero_mode(CLANK)`), out; Giant Clank (`hero_build(15)`), walks and punches, out: Ratchet where Giant Clank stood |
| `s7_magnet` on `s7-kalebo` (format 2, Kalebo III's base) | sequence 91 (the magnetic walk) filled from Veldin's core when he steps on the magnetic arc: walked up the wall drawn right (`RAC1_HERO_SEQS=0`: the old garbled model) |
| `arena_lava` on the arena's copy with `hero generic` | 0x68 in the lava, Cross: 0x69 out and back, left in it: lost, the level loaded again (the plugin's `level_end` / `level_start`); `arena_ch0k`, `arena_ch10k`, `arena_course`: the same waves and the course won as on Kalebo III's code |
| `s9_sky`, `s9_die` | Umbris's and Gemlik's worlds with their skies; Kerwan's crates (with its lift 1210) all there after a death |
| `s9_ship` (`gemlik`: `hero gemlik`, its ship 69) | not done: the ship's own code, spawned in the section, runs its return from a flight (Ratchet put out at 457.8 578.2, the ship gone) instead of waiting at its pad for "Enter ship" (Gemlik's own level state its level's setup makes: stage 10) |
| The original; the regressions | the reference pictures and RAMHASH unchanged; the arena's format 2 regression and stage 5's format 3 runs: the same RAMHASH as the branch's start (dev with the level mods' sky fix, which changed them from stage 8's); the equivalence test on all 19 levels |

Timings: a hero context taken 0.03 to 0.2 ms (the camera table copied, the cameras made again); a section switch into a hero
section 0.2 to 1.0 ms; a mode entered in under 0.1 ms; a sequence copied in its frame (32 to 39 KB); a core's hero sequences read
on the worker (134 of Veldin's) in 1.4 to 3.2 s while a level loads.

### Not done, and what stage 10 needs

- **Gemlik's ship**, **a level's HUD pictures**, **camera types without a hero context**: done in stage 10 ([Bosses](#bosses-stage-10)).
- A plugin's `hero_build` lasts until the next section switch (the entered section's line decides); a section with a board but
  no `hero kalebo` leaves Ratchet standing on it in 0x6b (Veldin's code has no riding).

## Bosses (stage 10)

DESIGN stage 10 (section 9's table): the game's bosses in a session, first the Snagglebeast. The code: `section_world.cpp`
(a world's HUD pictures, its light grid), `sections.cpp` (a world's level state, the `boss` lines, the bosses' soft reset and
win, the mod's rails in a world), `hero_modes.cpp` (the camera level; `RAC1_MOBY_WATCH` / `RAC1_MOBY_POKE`), `variants.h`
(`g_camera_level`), `leveldata.cpp` (the base's enemies in a world), the composer's `boss` lines and a world's rails
(`level_compose.cpp`), the SDK's ABI 13.

### What the design said, and what was done

DESIGN 9 listed per boss what it needs on top of stages 1 to 6. What the Snagglebeast needed was not boss-specific: it is what
every planet's objects need in a world's section, and it is now there for every world:

1. **The planet's camera code** (`g_camera_level`, variants.h): while a world's section is current its level's camera sites
   run (`on_camera_level(n)` for every level's camera types, camera hooks and callbacks: Umbris's, Blarg's, Batalia's... not
   only the hero contexts' five), and its own camera types are in the session's type table copy (stage 9's, now without a
   hero context: Umbris's 0x12, whose accept is its own and whose init / update / exit are Veldin's: a row's words are taken
   through the level's context, a table its map pairs with Veldin's keeps raw addresses in the image). A hero context's level
   gives the table when there is one.
2. **The planet's HUD pictures** (`world_hud_build`): the HUD's animations of the level the session's HUD data lacks (Umbris's
   and Gemlik's 0x7562, the boss bar with its portrait; Quartu's 0x7558, Giant Clank's energy bar; Orxon's 0x7553) are added
   to a copy of the session's HUD tables (animations, frames, palettes, textures: 0x17e4d8 +0x1c..+0x28), with their textures
   and palettes from the level's own set (the level file's part 6, decompressed: 327 KB in the heap), uploaded to the GS
   memory on demand as the game's level set is (the per-frame residence reset walks the copy's counts). Only animations whose
   frames are all the level set's are added; the tables are the HUD's while the section is current.
3. **The planet's level state** (`rows_put`): the level records the engine keeps under the current level number (the
   collected things 0x14c050, the killed-for-good bits 0x14c190, the group counts 0x14d590) and the level's live copy 0x15fc88
   are, in a session, the level mod's own (level_slots swaps them in). A world's objects now read and write a copy of their
   own planet's, kept by the session for the visit (zero at the session's start: a first visit), in while the section is
   current and while its objects are placed or placed again; the session's own for a card save (`sections_save_begin`) and
   when the session ends. The planet's real records (its own level number's row) are never touched. Gemlik's ship read the
   level mod's 0x15fc88 and ran its return from a flight: with Gemlik's state it waits at its pad.
4. **The planet's per-position grids**: gameplay_load points 0x15fbc0 (the objects' light grid), 0x15fb80 / 0x15fb50 (its
   lights) and 0x15ef80 (a camera block) at the level's gameplay file; a world kept the session's, and moby_update_light's
   grid lookup at the world's coordinates read past it (Gemlik's ship over the canyon: an out-of-range read). They are the
   world's state now.

And for the bosses themselves:

5. **`boss` lines** (`boss <class|name|level:class>...` in a section; the SDK's `boss_set` for an object of the plugin's): a
   death of Ratchet's in the section takes a boss still in play out and places it again (`spawn_objects` of its uid: its
   placement's record and pvars, the fight from its start; the plugin's `boss` callback `RAC1_BOSS_AGAIN` with the new
   object); a boss leaving play otherwise is **beaten**: logged, the callback `RAC1_BOSS_BEATEN`, its uid never placed again in
   the visit (a save keeps it out: the session item's gone objects). Nothing ends: what the original does after a win (the
   Snagglebeast: its bolts, its level's collected-thing flag, its checkpoint; no cutscene, no level change) happens in the
   world's state, and what follows is the plugin's (DECISIONS 8).

DESIGN 9 also expected the Snagglebeast's path reads, its HUD draw (a tag) and its checkpoint saves to need work of their own:
they did not (the registry of its level's data, the context's tags and stage 5's checkpoint hook covered them).

### The Snagglebeast

```
section cave remove respawn reset
  world umbris
  objects all
  box 195 355 25 320 500 90
  boss umbris.snagglebeast
  entry pit 263.35 483.16 36.5 -1.13
  checkpoint cave_start 263.35 483.16 36.5 -1.13
end
```

| Test (`s10-bosses`, `openengine/s10`) | Result |
|---|---|
| `s10_pit`: into the cave on its platform, idle | the boss wakes as on Umbris (state 0 to 3, then 3 / 1 / 4: its throws), facing Ratchet (yaw 2.01 on both); its health bar with its portrait (Umbris's HUD picture); Umbris's camera types in the table |
| `RAC1_MOBY_WATCH=1106:1` on Umbris itself (`RAC1_HERO_AT=605@1000`, the arrival skipped) and in the session, the same `RAC1_MOBY_POKE` (its last hit: state 14, health 0.5, one hit pending) 900 frames after Ratchet arrives | the death frame for frame: 411 of 412 frames equal (state, animation and its frame, place, yaw, +0x2c), state 10 with animation 8 for 410 frames, then 16, then out of play; before the poke the phases match once their random choices are taken out (the two runs' random numbers differ: one animation of two chosen at random, 6 frames apart after 400) |
| `s10_kill` / `s10_killdie` | beaten: `[section] ... its boss (class 1106, ...) beaten (1 in this visit); the session goes on`, the callback; the boss's own checkpoint save at its death is the session's ("game", 268.90 449.93 36.00); a death afterwards: back there, the boss not back (1 placed object back, 8 killed for good) |
| `s10_die`: its health set to 60 of 120, then a death | the soft reset at the checkpoint, the boss placed again (a new object, health 120, from its state 0), the callback `RAC1_BOSS_AGAIN` |
| a save after the win (`RAC1_SESSION_SAVE_AT`), loaded from Kerwan | back in the cave at the boss's checkpoint, the boss not there |
| the full fight played | not tried: the phases need a player (it takes its damage from the player's weapons and its own shock waves); `RAC1_SIM_INPUT=attack` on its platform does not reach it |

### Gemlik's ship (`world gemlik`, `objects 69 388`, `hero gemlik`)

It waits at its pad (state 1, its level's state); triangle boards it (state 2, then 4 flying, Ratchet in state 0x32); its
flight with the stick is Gemlik's own frame for frame (`RAC1_MOBY_WATCH=69` against Gemlik itself, the same input after the
same boarding: 760 frames equal to 0.01); a crash (state 7) is a death: the soft reset at the section's checkpoint and the
ship back at its pad (Gemlik itself goes back to its own checkpoint). Its flight ends at once with nothing to shoot at (its
draw counts the targets, `pv+0xe8`: Qwark's ship 388 must come with it); with 388 placed, Qwark's ship flies its route
(states 1 to 4) for the whole flight tried (1900 frames).

### The mod's own rails in a world's section (the arena's escape into Quartu)

`rail` / `rails` lines in a world's section (the composer: in the planet's coordinates; their points into the section's own
gameplay file; their strips into a copy of the planet's collision: its cells' faces once each, whole, with the strips, built
again, its hero groups kept): the runtime puts the level's rails first in the section's rail table and the mod's after them
(`sections_rail_first`: the SDK's rails find them). Test `s10-escape` (format 3 on Kalebo III's base, `hero generic`): a
rail in main through a portal into `world quartu`, the plugin attaching Ratchet to `quartu_in` in its `section_enter`
callback: he grinds the whole rail down (44 units) and lands on Giant Clank's arena floor (130.50 218.56 25.51); the hero
code main's generic one there; Kalebo III's robots (`base_spawn`, `base_hunt`) beside him walk up and hit him (they stood
still: a base enemy in a world's section now updates on the base's level data, `ld_enter`'s `LD_BASE_IN_WORLD`). Quartu's
collision probed (`RAC1_COLL_PROBE`, 6561 points of its arena) against Quartu itself: 6485 the same height; the others are
Quartu's placed objects the section does not place. A rail's model (`mesh`) is drawn in the world (above, "the modern
renderer"); test `rm-escape` (s10-escape with `rail quartu_in mesh ...`, a ring class placed in main and moved by the plugin
to the rail's top in `section_enter`, a second one spawned at its foot): the rail, the rings and Ratchet riding the rail in
GL and Vulkan pictures, the ride unchanged (`rail_find` 2, `hero_rail_attach` 1, the landing at 130.50 218.56 25.51), and the
same after main and back into Quartu (its kept level).

A world's section needs its planet's context, so the overlay maps' extents (`assets/generated/overlay_map/levelN.ext`,
stage 1's `extents.py`, made by `./setup.sh`): without them the log says `[ctx] level 15: ... .ext missing (./setup.sh): no
context` and `[section] quartu: level 15's context or registry cannot be made`, and the world comes without its level data
(cameras, paths, volumes) and its camera code.

### Not done, and what the other bosses need

- **The Snagglebeast in a mod's own arena**: it waits for Ratchet inside its level's cuboid 10 (its pvar +0x1a4) and follows
  its routes (17 path reads) at Umbris's coordinates; a mod's arena needs a profile binding those slots to its own volume and
  paths (stage 4's `object_bind` kinds `area` / `path`), the camera level for its section (a `camera <level>` line or SDK call
  setting `g_camera_level` without a world) and the HUD pictures likewise (`world_hud_build` from a level other than the
  section's world).
- **Qwark's space fight** (Gemlik, 388): with the ship working, what remains is 388's closure (it spawns class 401, which
  the measured spawns do not list: `data/pool/spawns.txt`'s `13:388` line needs 401, and the deps made again), its three camera
  requests (Gemlik's camera type 0x13: in the table with `world gemlik`), its HUD element 0x11 (the HUD picture path above),
  then the fight played to its end (its death and what Gemlik does then: the beaten callback with a `boss 388` line).
- **Nebula G34's creature** (Blarg, 1051 with 857 / 302 / 303): Blarg's world as a section with `objects` of its room; its
  camera type 0x16 / Blarg's 0x12 now come with the camera level; its HUD element L6:0x249738's pictures with the world's HUD
  copy; its 7 group reads (the world's groups, stage 8); then the same boss line and tests.
- **Giant Clank's fight** (Quartu): `hero quartu giantclank` exists (stage 9); his energy bar now has its picture in
  `world quartu`; the drones (44, 655) with `objects`, their paths from Quartu's registry.
- **Drek** (Veldin finale, 1422): its 7 cutscene starts and the game's end (the credits) must be cut off by a profile hook
  (DECISIONS 8), its 11 group reads.
- A world's HUD copy takes the level's whole set (about 330 KB a world); a boss's music changes, if any, are its planet's
  music in its world's section.

## The board outside a race

Kalebo III's and Rilgar's board code is their race's. Its riding state compares the game's level number (on a level mod its
base's: 16 on Kalebo III's base, whatever world the section shows): on 16 a z under 76 is a fall (state 0x6e, Rilgar's 61.4 on
5), and a fall or a wipe-out (0x6d, 0x6f) ends "back on the track" (L16:0x225ac0, L5:0x254358: the nearest point of the
board's race path, the board's pvars +0x24 an index into the level's path table); at the mount it asks for the race HUD (the
HUD requests 5 and 7: lap, time, place) and its race code (0x225d70) counts laps on the path its pvars +0x20 name. What the
arena saw follows from that:

- **`world quartu`** (Quartu's arena floor at z 26): every frame a fall (26 < 76), "back on the track" at a point of Quartu's
  path table (the board's index into Quartu's paths: 248.2 386.9), still under 76: 0x6e for ever; the race HUD (lap 1/3,
  the time, 6th).
- **main on the mod's own geometry** (above 76): it rides; the first fall or wipe-out (the arena's 0x6d after 60 frames:
  `forward` held from the mount drove him into a wall; `forward` itself is not the cause) went "back on the track" of a
  path table with no such path: (0, 0, 0.01), under the death height, so a death and the soft reset at the checkpoint, on
  foot. A bug (the race's logic on a level that has no race): fixed below.
- **`world rilgar` with `hero rilgar board`** (Rilgar's finish line at z 73.6): Rilgar's code read 16, so Kalebo III's 76: a
  fall at once, for ever (the "wait for the start" the arena saw).

The fix (hooks in both builds, tools/codegen/hooks.glue, `level_mod_hero.h`; the original's levels as transliterated): on a
level mod's level the compares see **the board's planet** when the current section's world is Kalebo III's or Rilgar's
(`world rilgar`: 5, its own heights and start line, its race as on Rilgar), and elsewhere the board is **free**
(`hero_modes.cpp`): its race path none (pvars +0x20 -1, the player's +0x8b4 0: no laps), no race HUD at the mount, its own
fall test (under 2 units over the death height, or the plugin's `board_fall`), "back on the track" replaced by where he got
on (or the plugin's `board_return`) for a fall and by where he rode 40 frames before for a wipe-out. The plugin's
`board_rule` forces either; `board_touching` lists the mobys his body touches on the board (the game's `mobys_in_sphere` on
their collision), for the plugin's `hit_send` (the SDK's ABI 16).

```
[hero] on the board at 134.60 235.00 25.51: free (no race: no laps, no race HUD, a fall back where he got on or at the plugin's place)
[hero] the free board: a fall (23.99 under 24.00)
[hero] the free board: back on it at 134.60 235.00 25.51 (where he got on)
[hero] on the board at 275.48 334.34 73.60: the race of its planet's world
```

| Test (local test mod, format 3 on Kalebo III's base, `hero generic`) | Result |
|---|---|
| `world quartu`: `hero_build(16)`, `base_spawn(439)`, `hero_mode(BOARD)` at 159.9 241.7, stick forward | rides (0x6b) across the arena floor at z 26, no race HUD; a wall at 172.2 250.2: a wipe-out (0x6f), back 8 units before it, riding again |
| the same from 134.6 235 facing west | off the floor's edge: a fall under 24 (Quartu's death height 22 + 2), 0x6e, back on the board where he got on, again each time |
| main (the plateau at z 100), `board_return(205 190 100.5)` | off the edge: a fall under 72, back at the plugin's place, riding on (before: (0, 0, 0.01), a death, on foot at the checkpoint) |
| a robot (1445) in the board's way, `board_touching` + `hit_send` | touched once at 1.1 units, knocked down (state 8) |
| `world rilgar`, `hero rilgar board`, stick forward | Rilgar's race: rides the track from the finish line, its race HUD (lap 1/3, the time, 1st), a fall under 61.4 back on its track (before: 0x6e at the start for ever) |

**The race camera.** The board's race camera (camera type 8, the same code on both: L5:0x329358 and L16:0x2f3e60, called by its
update) keeps its target over a floor chosen by the level number: on Rilgar 62.4 within 48 units of its big ramp (286.16
447.82), else 67.2; on Kalebo III 77.5; none elsewhere. In `world rilgar` on Kalebo III's base it read 16: the target held at
77.5, so the camera at 78.89 wherever Ratchet was (the arena's report: "the camera goes up, framing the floor"): 5.2 to 6 units
over him from the finish line (z 73), 2.8 to 3.0 on the track at 76 (Rilgar 2.0 to 2.2), 17 to 18.5 in a fall (Rilgar 7 to
8: the 67.2 floor). The compare now sees the current section's world (`level_mod_camera_planet`, hooks in both builds; none
in main and the mod's own sections, so no floor of the base's on the mod's geometry; the original's levels as
transliterated). The higher camera in a fall is Rilgar's own: on Rilgar too it stays over 67.2 while he falls under the track.

| Test (`RAC1_HERO_STATE=1`: Ratchet and the camera each frame) | Result |
|---|---|
| native Rilgar (`RAC1_GOTO=5`, `RAC1_HOVERBOARD_AT`, stick forward) against the arena's `world rilgar` (`arena_go`, `ARENA_SEC=rilgar`, the same stick), the frames where Ratchet is at the same place (to 0.01) | 1039 frames (riding 259, wipe-outs 354, falls 426): the camera the same to 0.000 (before: 0.80 riding, up to 10.3 in the falls) |
| the same, from the finish line | 2.0 to 2.2 over him (before 5.2 to 6.0 for the first 150 frames) |

## A world's zones: what is drawn (`zones all`)

What a world's section draws of its planet is the planet's own culling: the zone grid (`0x15f600`, gameplay's: cells of 4
units, each a 128-byte list of the zones seen from it) gives the visible zones `0x173d00` for the camera's place each frame
(`zones_from_camera`, 0x1f7408: the cell's list, else its neighbours', else the last list `0x15f610`, else all), and every tfrag,
tie, shrub and moby whose zone bit (+0x3a, +0x18, +0x36: the gameplay file's +0x8c lists) is not in it is not drawn. The zone
mode `0x16c074` (one of a world's state words) is 2 with a grid, 0 for all zones (`zones_update`, 0x1f77f8). Quartu's Giant
Clank arena (around 170 240 26) is no special case: no Quartu code changes the grid, the mode or the masks (its classes' and
hero code's builds name none of them), and in `world quartu` it is drawn and solid from the camera's places tried (the board
on its floor, fixed cameras inside it at 34, 45 and 60 high and over it from 150 200 70 and 130.5 180 66; GL and Vulkan): from
inside the arena at 150 236 28 its cell sees 78 of the 124 tfrags within 50 units and 77 of the 118 ties, the rest behind its
walls. On Quartu itself at its start Ratchet put at 140 215 27 falls through (there is no floor there: the gap between the
platform and the arena), and walking from the platform he meets the arena's wall: the arena is reached as Giant Clank, whose
camera (122.8 222.6 40.6) sees 103 of those tfrags.

A section's `zones all` line (composer 8: `zones <section> all` in `sections.txt`) sets the zone mode to 0 while the section is
current (`apply_state`, after its world state): every zone drawn wherever the camera is, on every renderer (it is the game's
own visibility, before any drawing). At 150 236 28: 124 of 124 tfrags and 118 of 118 ties; the pictures there the same within
82 pixels (the sky's stars). For a camera the planet never had, or one off the grid whose last list is another place's.

## A world's objects: what they spawn, which class they are, their effects

Three things found with the arena's escape into Umbris's cave (`sections.cpp`, `section_world.cpp`, `leveldata.cpp`,
`moby_spawn` in `ovl_24_25.cpp`).

**An object whose code changes its class id.** A section knew its objects by slot, uid (+0xb2) and class id (+0xa6). Umbris's
class 529 (Ratchet's ship parked on the planet: its update queues the ships' canopy draw, L7:0x2ba260 = Veldin's 0x2935b8)
writes 533 into its own +0xa6 at its first update, as every ship class does (0x212..0x215). Its object then looked gone:
leaving the section did not take it out, and it stayed in play in main with Umbris's class block under the session's own ship
id 533 (the arena's ship): at the first frame there `moby_update_visible` -> `moby_anim_advance` read an animation pointer out of
range (`Pointer resolution out of bounds: 0x131c0657`; the arena's 0x7a020000). It showed only with the arena's `update_all`
line: without it the 529 is out of its update range at the cave's entry and never runs. An object is now known by its slot and
uid with its class id or, when its code changed that, its class block (+0x24 as placed).

**What the objects spawn.** `moby_spawn` tells the sections (`sections_moby_spawned`, while a session with sections is on)
which moby's update or callback runs (`g_spawn_updating`): a moby spawned by one of a section's objects, or by one of those,
is that section's. Leaving the section (`remove`) takes them out with the objects; `freeze` freezes them too; a death takes a
boss's out before it is placed again (before: the Snagglebeast's three 1046 stayed beside the new boss's). In the arena's
cave (`objects all`, 109 objects): 24 to 67 such mobys at the return to main (the debris and bolts of crates 13, 14, 226, 315,
357, 358, 806, the boss's 1046), all of them before left running in main on Umbris's code.

**Same-id classes.** The pool's block hashes said a class id is the same class on every level; the code was not compared.
`tools/overlay_match/class_code.py <levelA> <levelB> [class...]` compares a class's update function on two levels on the
disc's own MIPS, instruction by instruction with the relocated fields masked, each jal and lui + lo address classified (the
base executable's, the engine's data or function through the map and its extents, a label of the same function, a function
of the level's own compared in turn, own data), every function it calls followed. Against Kalebo III, of the 108 to 118 ids
each level shares with it, every one with an update function has the same code but 1134 on every level (a helper it calls,
built otherwise in Kalebo III), Eudora's 605, Rilgar's 439 (the board) and Veldin 2's 638 (the same: engine helpers each
build compiled its own way). Umbris's 605 (an invisible zone marker, no model: novalis_props.cpp's zone_marker_update_at; the
hero's camera may turn to the nearest within 20 units), 805 (the checkpoint), 1135 (a teleporter pad), Quartu's 605 and
Rilgar's 1139: the same block, the same textures (the MRP's texture data), the same sounds (the banks' samples), the same code.
Placed as the base's class they are the planet's objects (their placements' pvars are the planet's); there was nothing to
import under another id. What still differs is the engine's: the level number they read is the base's (605's hint row
0x14bf10 + level * 16, 805's checkpoint rows) and the class table's +0x2c word is the base level's (one list for all its
classes).

One caveat of stage 10 was a real difference: a world's own objects of a class the base has a profile for (Gaspar's 52 on
Kalebo III's base) updated on the base's level data (`LD_BASE_IN_WORLD`), meant for the base's enemies a plugin brings into
the world. They now keep the world's (`sections_world_object`): Gaspar's 52 by its start in `world gaspar` stays as on Gaspar
(state 1) where it read Kalebo III's paths and walked off (state 4).

**The effect textures.** The level's effect textures (the asset index's +0x58 count, +0x5c entries, +0x68 data: the
particles', the Snagglebeast's fire rings and flames, drawn through `effect_tex0`) were the base's in a world's section. The
first 40 are the same on every level; the rest are each level's own (Umbris 79, Kalebo III 56, Aridia 106; Batalia shares 54
with Kalebo III). The rings (class 1046's draw callback, L7:0x30dcf0) take one past the 40: on Kalebo III's base nothing or
the wrong picture was drawn. A world's build now copies its effect texture block (Umbris 0.4 MB) and makes the table as
level_setup_body does (`sub_0023f768`); the data word 0x15f420, the count 0x15f41c and the table 0x16d180..0x16d880 are part of
the world's state (their cached TEX0s cleared when it is swapped in or out). With Umbris's: the shock waves' fire rings as on
Umbris itself (pictures at the same frames after the arrival, 60 to 360: on Umbris, before only sparks at the boss's feet, now
the rings).
A base enemy in a world's section (`base_spawn`) draws its own effects past the 40 with the world's pictures.

## A world's looks and its cameras: the planet's own, by what its code reads

Umbris's cave in `world umbris` (the arena's "La grande fuga", the request with its pictures beside native Umbris at the same
place) had a flat orange lava, its ceiling's ring out of the picture and its camera on the other side of Ratchet; Batalia's rail
(`world batalia`) shook its camera, 1.2 to 1.4 units one frame and 0.2 the next. Each was a part of the planet's state its
own code reads that a world's section did not give it, and each is now given for every world's section:

1. **Its classes of code alone** (composer 9: level mods must be set up again). A class with no block in the level's asset
   data but an update function (the pool's row with no data) was "not placed" by the composer. Umbris's lava is no tfrag: it
   is VU1 program 7's sea drawn by an object of class 460 (its draw callback, `L7:0x2f8038`, its sea at `L7:0x1d3400`); under
   it the world has only the orange it is drawn over. Such a class is now imported as its level's setup registers it, with no
   header (`moby_class_register`'s own case; `session.cpp`'s `import_one`), and placed when `objects` names it (or takes
   `all`). The cave's sea is 460, in the arena's box; class 1018 is Umbris's other sea, outside it.
2. **Its effect textures** (the section above): the 460 sea's frames are Umbris's effect textures past the 40 (the lava's two
   animations of 16 palettes each); with Kalebo III's in their place the sea was black.
3. **The level data's words naming a placement** (`sections.cpp`'s `world_camera_links`). The gameplay file's +0x50 list (the
   pvar words naming a placement, which the objects' get as the slots of the objects placed) names the cameras' too, and the
   level's own load leaves those as they are: there a placement's index is its slot. Umbris's camera 0 (type 0x12, its pvar
   +0x28) names placement 541, the Snagglebeast; Umbris's camera code (`L7:0x3287e8`) turns the follow camera toward that
   slot's moby (`cam0_turn_point`). In a section the boss is in another slot, and the camera turned toward an empty slot's
   (0, 0, 0): from Ratchet at 263.35 483.16 facing -1.13 it swung to 268.0 491.8 instead of the native 259.2 492.0, and the
   ring, which is the world's (a tie up in the cave's roof), was out of the picture. Those words in the level's registry are
   now the slots of the section's objects placed from them (-1 never placed; a slot kept once out of play), made again at
   each placement (a death's soft reset places the boss again).
4. **Its rails, the same table for its code and the engine's** (`ld_rails_put`). A world's section's rail table (the level's
   grind lines first, the mod's after them) is the engine's 0x15f70c; its level's own code reads its registry's (the
   context's view of 0x15f70c), a copy of the same rails from the pool's level data. Batalia's rail camera (type 3, its accept
   in the engine: the hero's rail's points 0x13f8b0 against the table's) took over in the engine's test, then its own
   pre-update (`L8:0x312a28`) compared the registry's copy, asked out (+0x7e 3), and the follow camera took over again: every
   other frame, the camera alternately carried by one and the other. The mod's rail was not the cause: Batalia's own rail 2
   with no rail of the mod's shook alike. The registry's rails are now the section's table while it is current.

| Test (local copies of the arena, format 3 on Kalebo III's base, `hero generic`) | Result |
|---|---|
| `world umbris`, `objects` the arena's and 460, Ratchet at its entry (263.35 483.16 36.5, -1.13), against Umbris (`RAC1_GOTO=7@60 RAC1_HERO_AT=-1@4500,...`) | the camera 259.17 492.01 39.44 (native 259.24 491.87 39.40), turned toward the boss at 278.40 451.23 as on Umbris; the lava's sea animated (GL and Vulkan); the ring in the picture; the boss in view |
| the same, the boss's camera with only `objects 1106 460` | the same camera |
| `world batalia`, down Batalia's rail 2 from 378.34 170.07 (the planet's collision, then the composer's copy with the mod's strip, then on the mod's rail `batalia_in`), the camera each frame | the largest step of the camera 0.57 (Batalia itself 0.57; before: 1.38, every other frame), the rail camera (type 3) held as on Batalia |

## What the next stage builds on

- **Hero contexts (stage 9)** and **the camera level (stage 10)**: a planet's own hero code and camera code in its world.
- **A world's planet state, HUD pictures and grids (stage 10)**: its objects behave as on a first visit to it.
- **Not done**: a world's objects outside the chosen set do not come (the planet's glass and water on Kalebo III are objects);
  the GS's VRAM image and the chrome / glass map registers stay the session's; the renderer's first entry into a world takes
  one frame of 43 to 73 ms; the mod's own meshes are not drawn in a world's section (its own rail models and objects are);
materials and emissive lines (lights.txt) do not reach them there; a world on a session
  base other than Veldin's and Kalebo III's was not tried.

## Putting a section back (ABI 17)

A section's placed objects killed "for good" stay so for the whole visit of a session, and a boss beaten stays beaten
(`dead`, `beaten` of the section, and the records the game's spawn rule reads); a save keeps it. A challenge played again
(the arena's Great Escape through Batalia's mines and the Snagglebeast) found the boss already beaten and the mines
destroyed. The arena worked round it by creating them at each try.

**What a section remembers** after it is left, and what `reset_section` (`sections.cpp`) puts back:

| Memory | Where | Put back by |
|---|---|---|
| the placed objects killed in the visit | `Section::dead` (by uid), `dead_imp` (its other levels' classes), and what is not alive at the leave | cleared: placed again at the next entry |
| the bosses beaten | `beaten`, `beaten_imp`, `beaten_n` (`boss_beaten`) | cleared: 0 |
| what is in play of it | its objects (frozen, kept), the mobys they spawned (`kids`), the plugin's named bosses (`pboss`), its pvars' heap blocks | taken out (`moby_retire`), freed |
| the level's records the spawn rule reads | a world's: the rows of its world's level (`g_rows`: the collected things, the killed-for-good bits, the group counts); a section of the mod's own: its placements' bits in the session's rows (the card's) | a world's rows erased (a first visit's: zero); an own section's uids' bits cleared (`Section::puids`, taken at its commit) |
| the game's visit tables | `0x1ba4d0` (killed for the visit, a bit per uid), `0x1baa24` (live state byte), `0x1bb684` (the checkpoint's copy: a boss's death at its own checkpoint keeps it killed) | cleared for the section's uids (a world's `wuids`, an own section's `puids`) |

The last two rows were found by testing: the first version cleared the section's sets and a world's rows and the Snagglebeast
still did not come back (its death had written the checkpoint's copy `0x1bb684`, which the spawn rule reads first).

- **`section_reset(name)`** (ABI 17): refused (0, a line on stderr) for no session with sections, no such section, **main**
  (its objects are the session's own `g_rt.placed`, put back by a death's soft reset and kept by its policy), **the current
  section** and one a switch into it is pending. For any other section, in whatever state (not in, in and frozen, kept running,
  left removed): done at once, the objects placed again by `spawn_objects` at its next entry (or its streaming's commit, for a
  section dropped with `section_unload`: its `puids` kept).
- **`fresh`** (level.txt: `section <name> [remove|freeze|keep] [respawn ...] fresh`; `sections.txt`: a line `fresh <index>`):
  `do_switch` resets the section it enters before it is entered (its state is applied after, so a world's rows are the
  first visit's). A world section's first entry is not reset (its rows are the session's own, empty at the arrival, its objects
  as placed); a section of the mod's own is reset at its first entry too, as a save's load can have brought the card's bits
  (crates broken before the save) with it. The composer writes the line only for a section that has it: the output of every
  existing mod is unchanged (composer version 9 still), a build made by a composer without `fresh` is a build without it.
  Not a death's: `respawn` decides what a death does inside the section.
- **Saves.** A section put back is a section with nothing gone: `sections_gone_of` (the save's session item) lists `dead` and
  the objects not alive, so after a reset the item says 0 placed objects gone, and a `fresh` section's list is empty
  whatever happened in it (`sections_gone` ignores such a list too). An own section's card bits are cleared by the reset, so
  the records the game writes are the fresh ones. A world's rows are not in the card at all (a card save puts the session's own
  in: `sections_save_begin`).

| Test (local mods `sreset-test`: format 3 on Veldin's base, `world umbris` with `objects all`, `box`, `boss umbris.snagglebeast`; a section with three crates of the mod's own; `RAC1_MOD_TEST=sr_*`) | Result |
|---|---|
| `sr_noreset` (control): the Snagglebeast taken to its last hit and its crates hit as props (14 broke), out to main, back | beaten (1), back in with 103 of 118 objects and no boss: what the arena found |
| `sr_world`: the same, then `section_reset("cave")` from main, in again, twice | each entry the same 118 objects (the boss, 500 x26, 501 x4, 511 x19 ...), `boss_beaten` 0, the boss beaten again each time (1) |
| `sr_freeze` (policy freeze): the boss mid-fight, out, reset | `104 objects in play taken out, 40 mobys they spawned`, in again: 118 objects, the boss at its health 120, state 3 |
| `sr_own`: three crates (500, 500, 501) broken, out, in (none there), reset, in | 3 again; broken again the same |
| `sr_fresh`, `sr_own_f` (sreset-b: `fresh` on both sections), three rounds each | at every entry the same objects, the boss to beat each round; the first entry is not reset (118 objects), a section of the mod's own is |
| `sr_refuse` | `section_reset("main")`, `("nope")`, `("")` and the current section: 0 each, the log says why; from main: 1 |
| saves (one run saves into slot 3 of a scratch card, a second run loads it from Veldin: `RAC1_SESSION_SAVE_AT`, `RAC1_GOTO=0@60 RAC1_SESSION_LOAD_AT=300:3`): after a kill, no reset | `15 placed objects gone`; loaded: 103 objects in the cave, no boss, `boss_beaten` 0 |
| after a kill and `section_reset("cave")` | `0 placed objects gone`; loaded: 118, the boss there, beaten again when fought |
| `fresh` section after a kill | `0 placed objects gone`; loaded: 118, the boss there |
| own section, crates broken: no reset / reset / `fresh` | `3 placed objects gone` and loaded without crates / `0` and 3 crates / `0` and 3 crates (a save's card bits cleared at the entry) |

Limits found on the way, both since looked into (the branch `fix/session-load-crashes`):

- **A load made while a session is running** (the pause menu's Load, in the same process) crashed entering a world section
  afterwards, in `unfreeze` -> `moby_update_transform`, idle save or not. The cause is not a dirty slot: `unfreeze` (and the
  main section's `session_rt_main_enter` and the soft reset's restore) transformed every object put back in play, including
  one of a class with **no block of its own** (`+0x24` 0: a class of code alone, such as Umbris's 258 and 1142), which no
  placement ever transforms (`section_spawn_placements` and `session.cpp` guard it). The transform of such a moby reads the
  class's animation table at EE address `0 + 0x48 + 4 * the animation` (the animation index is 0, the cached one `+0x71` 0xff),
  low memory that held something harmless in the runs without a load, and a float after one went through the game's own
  level exit (`0x3f9699a0`, read as an address: out of range). So a first arrival never failed. Fixed:
  `moby_update_transform_placed` (session.cpp) skips a moby with no class block, used in `unfreeze`, `session_rt_main_enter` and
  the respawn's restore. Tested with `RAC1_SESSION_SAVE_AT` / `RAC1_SESSION_LOAD_AT` and a `RAC1_SIM_INPUT=seq:` pressing Cross at
  the planet prompt the load lands in (the Load page's end: `level_exit_to`), in the same process: arrival in the cave, back to
  main, save, load, the cave again (the crash before the fix, the cave and its boss working after it, a death and its soft reset
  included), and the same with the save made inside the cave.
- **Two sections of the same planet** (`cave` and `cave2`, both `world umbris`) are **refused by the composer**: `level.txt:N:
  section cave2: level 7's world is already section cave's ...`. A world's level state is per world level, one copy in the game:
  its moby group lists (the engine's group table of the level, `world_groups`), its cameras' links (`world_camera_links`), the
  rails and the host meshes drawn in it, the rows of its records. The second section's streaming and its objects' groups
  overwrite the first's in that table, and the first one's leaving empties them (`groups_clear` zeroes the entries of its
  `groups`, whoever owns them now), so the objects of the section in play read a group list that is gone. Measured on the arena
  mod's Umbris (two `world umbris` sections, a boss beaten in each): after the first boss's death the other section's frozen
  objects were in play in main (`+0x34` bit 2 cleared) and, in the second section, its objects' groups (2, 3, 25, 66, 77, 81..83)
  had lists of 0; `moby_visible_queue` walks a marked group's list of ids from the table, here from address 0, so garbage ids reached the
  look list (`0x1aba00`) and `hero_look_at_update` read a moby that is not one (`std::out_of_range`, 0xba96c291), 15 frames after
  the second boss was beaten. Supporting it means a table per section (the groups and the camera links put at each entry and taken
  out at each leave, the rows and the host meshes by section, not by level): not done, one section per planet is refused with its
  reason instead of crashing later; the several places of one planet are one section with several `entry` lines and a `box`
  that holds them all.
