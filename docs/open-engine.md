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
by **portals** in one frame, streamed in on a worker thread and committed a little each frame, with no level load.

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
3. **Nothing else**: the weapons, the ammo, the bolts, the items, the session's imports and contexts, the plugins' state and
   the objects they started (base level objects, imported ones, the level's own classes), the spawn pools, the frame clock.
   No level load (one `[levels] level 16's header` line for the whole visit).
4. **The plugins' `respawn` callback** (SDK ABI 9) with the checkpoint: what the battle needs back is the plugin's to decide
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
has it.

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

## What stages 7 and 8 build on

- **Stage 7 (rails, Magneboots, set pieces)**: the rails are level data per section: the session view's rail table (0x15f70c,
  0x15f710) should be swapped at a switch as the collision is (`apply_state`); a section's gameplay file can carry grind lines
  (+0x74) as the game's does, `section_spawn_placements`' sibling to place them. ABI 7 is still reserved. The landing gate,
  the Magneboots surfaces (collision types per section: they come with the section's own collision already) and the camera
  service are not started.
- **Stage 8 (game worlds as sections)**: a section of a game level's scenery needs (a) its world as host meshes (the MRP
  world export into the section's own range of `host_meshes.mrpc`: the meshes' section tags and mask are there), (b) its
  collision (a game level's block: `collision.bin` is the game's format already, so a level's own block can be copied as the
  sky is), (c) its placements from the level's own gameplay file: `section_spawn_placements` places a game level's gameplay
  as the native load does (the equivalence test), but a section of another level needs its uids mapped to session uids
  (DESIGN 7.4: section index x 4096 + uid; today the composer numbers a mod's own sections' uids after main's), its moby
  groups placed (+0x48: not yet) and its classes imported with their contexts (stage 3/4's), and the level's own level data
  per section (paths, cameras, volumes in the registries: `ld_*` has no section owner yet). The sky and lights of a game level
  per section already work (`sky <level>`, `lights`).
- Not done: merged collisions (one collision for far-apart sections: a section without a `collision` line keeps main's,
  which is the cheap form); per-section `object` / `trigger` lines (the level's own classes' objects and triggers are
  session-wide); a section's own music files (a game level's tracks or none); a section's `path` lines' base-view paths are
  not freed with the section; streaming the meshes' GPU data (the host meshes of every section are built with the level's
  world at the arrival: 54 KB for the test). (`start cave:door` tested: after the landing the switch to the cave, Ratchet at
  its door.)
