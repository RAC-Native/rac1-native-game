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
from six planets, fight in the arena example's test copy.

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

## What stage 5 builds on

- `ld_registry_make` / `ld_path_reserve` / `ld_group_create` / `ld_enter`, the profiles (`profile_find`,
  `profiles_session_spawn`), `ld_placement` (any placement of a level by uid), the incremental renderer import.
- Not done yet: groups of a level are none in its registry (a class that needs its group's mobys gets them only from a mod's
  `group_create`); the twin run captures paths, not groups; cameras and the 0x90-byte records are copied but no class tried
  needed them moved; rails are readable in a registry, placed by mods in stage 7; profiles are written by hand from the
  generated code (which pvar words index which table) and the twin run.
