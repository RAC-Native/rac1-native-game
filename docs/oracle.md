# The oracle

The oracle is how a port is proved. It runs the game's original code and our C++ port on the same
memory, and checks that both leave the same memory behind and return the same value. A port that
does not match is not done, however plausible it looks.

It lives in `tests/oracle/`. It is a developer tool: players never build it, and it needs game data
that is never in this repository.

## What it is

The oracle is one program, `ovl_oracle`, made of many **batches** (a second, small one,
`vecmath_oracle`, checks the vector math library the same way). A batch is one file,
`tests/oracle/batch_<N>.cpp`, that checks the ports of one piece of work: a level's classes, a
menu, a family of particles; the first ones (1 to 58, 60, 62 and 64, among them the hero batches
44 to 55) are together in `tests/oracle/ovl_oracle_unit.cpp`. Each check in a batch names an original function by its address and
the C++ function that ports it, and runs both many times:

1. The EE's memory is set up as the game has it at some moment (see below: from the disc, or from
   a RAM dump of the game running).
2. The check varies the state the function reads: a moby's state byte, a timer, the pad, the
   random seed, a position. Every case is different.
3. The original function runs in the R5900 interpreter (`runtime/ee/ee_interpreter.cpp`), from the
   game's own MIPS code. Its memory and its return value are kept.
4. The memory is put back as it was, and the port runs on it.
5. The whole of the EE's 32 MB of RAM and the scratchpad must be identical, word for word, and so
   must the return value. A difference is printed: the address, the original's word, the port's.

The interpreter is not an emulator. It has no timing, no interrupts, no GS, VIF or IOP; hardware
registers read as 0. It runs one function, or a frame's worth of game code, from a memory image.
The float arithmetic is the EE's (rounding toward zero, no denormals) on both sides: the oracle
switches the host's FPU to it before it starts, as the game does (`runtime/ee/ps2_fpu.h`).

The memory a batch starts from comes from one of these:

- **The disc.** The boot program and a level's overlay are read from the player's own disc
  (`./setup.sh` puts them in `assets/`), and the level is laid out as the game does when it starts
  it. Most batches need nothing else.
- **RAM dumps of the game running.** For a state that is hard to build by hand (a boss fight, a
  cutscene, a menu open, a race), a headless run of the game writes its whole EE RAM at a given
  frame (`RAC1_RAMDUMP`). The batch loads the dump and runs both sides on it.
- **Captured VU states.** The VU microprograms' batches start from the VU registers and memory
  the game had at a kick (`RAC1_VU1_CAPTURE`, `RAC1_VU0_CAPTURE`).

The dumps and the captures are game data. They are never committed (see [The dumps](#the-dumps)).

## Why it exists

The project ports the game function by function, from the original code, and invents nothing
([decisions 2 and 3](decisions.md#2-port-function-by-function-from-the-original)). Early on, some
"ports" were really reconstructions: they behaved plausibly, but their constants were tuned and
some came from the wrong original function. Nothing compared them with the original.

The oracle closes that gap ([decision 6](decisions.md#6-an-r5900-interpreter-as-the-porting-oracle)).
Ports are proved, not guessed: a port either matches the original on recorded states, or it is not
done. The same idea proves the VU microprograms against the VU interpreter, and the native
renderer's assets against the faithful path ([Rendering](rendering.md#how-the-mrp-is-checked)).

## How it was used in the development repository

The game was ported in a private development repository. There, every port came with its batch:

- one batch per piece of work, numbered in order (batch 59 is the level start, 120 Batalia's
  classes, 180 the shared weapon classes);
- the batch was run, with the batches the change could touch, before the work was merged;
- a level's own code was checked on that level's own overlay (`ORACLE_LEVEL`), since every level
  runs its own copy of the shared code ([decision 8](decisions.md#8-every-level-runs-on-its-own-overlay)).

The first states were savestates of PCSX2 running the game, saved as RAM images. Later the oracle
learned to lay out a level from the disc by itself, and the harder states were taken from headless
runs of the port, once the code that leads to them was itself proved.

## How it is used here

From now on, the same rule holds in this repository:

- **Every change to ported game code ships with a batch.** A new port gets a new batch; a fix of a
  port gets new or changed checks in the batch that proves it.
- **The batch passes before the change is merged**, and so do the reference batches below.
- Host code (`src/host`, `src/game/render`, `src/game/enhance`, `runtime/`) needs no batch, but it
  must not change the game's state: the reference images in [Status](status.md) show it.

## Building and running

The oracle needs the generated sources and the disc's files, so run `./setup.sh` first, with the
**NTSC-U disc** (`SCUS-97199`): the batches name the original functions by their NTSC-U addresses.
Then build it into the same build directory:

```sh
cmake -B build -DRAC1_ORACLE=ON
cmake --build build --target ovl_oracle -j 3
```

`RAC1_ORACLE` is off by default, and `./setup.sh` never turns it on. Once it is on in a build
directory, a plain build of that directory compiles the oracle too. It builds on Linux; on Windows
it compiles with the same code, but it has not been tried there.

Run it from the repository's root (it reads `assets/`), one batch at a time:

```sh
build/tests/ovl_oracle --batch 63                  # a batch on Veldin (level 0)
ORACLE_LEVEL=8 build/tests/ovl_oracle --batch 120  # a batch on level 8's own overlay
```

It prints one line per check (`ok (40 cases)`, or the differences), then `PASS` or `FAIL` with the
number of failing functions. The exit code is 0 for a pass, 1 for a failure, and 77 when the disc's
files, or the level's address map, are missing. Each batch's header says which levels it runs on and what it needs.

Useful variables:

| Variable | What it does |
|---|---|
| `ORACLE_LEVEL=n` | Runs on level `n`'s own overlay (default 0, Veldin). Same as `--level n`. |
| `ORACLE_ONLY=<name>` | Runs only the check of that name. |
| `ORACLE_FOCUS=<name>[:<case>]` | Runs only that check (or one of its cases), with the same random cases as a full run. |
| `ORACLE_SEED=n` | Another random sequence: other cases. |
| `ORACLE_BAD_MAX=n`, `ORACLE_DIFFS=n` | How many failing cases are described, and how many differing words each. |
| `ORACLE_STACK=1` | When the original stops in the interpreter, prints where. |
| `RAC1_ORACLE_DUMPS=<dir>` | Where the RAM dumps are (default `assets/dumps/oracle`). |

### The reference batches

Before a change to game code is merged, its own batch passes, and so do these. They cover the
shared code every level runs:

| Level (`ORACLE_LEVEL`) | Batches |
|---|---|
| 0 | 63, 79, 80, 84, 85, 89, 95, 103, 104, 106, 110, 113, 115, 118, 123, 125, 130, 141 |
| 1 | 91, 97, 103, 113, 115, 123 |
| 2 | 107, 109, 123, 137 |
| 3 | 112, 123, 137, 138 |
| 4 | 111 |
| 5 | 114, 123, 140 |
| 6 | 117, 141 |
| 7 | 119, 142 |
| 8 | 120, 146, 160 |
| 9 | 121, 150, 170 |
| 10 | 122, 123, 171 |
| 11 | 124, 146 |
| 12 | 192, 200, 202 |
| 0, 8, 11, 12, 13, 14, 15, 16, 17 and 18 | 203 (the PDA, hero item 0x20) |
| 13 | 123, 220, 222 |
| 14 | 44 .. 55, 221, 223 |
| 15 | 240, 241, 242 (and the hero batches 44 .. 55, 123) |
| 16 | 250, 242 (and the hero batches 44 .. 55, 123) |
| 0 and 3 | 180 |
| 0, 1, 3 and 5 | 190 (it proves a fix of the code generator, `tools/codegen/mips2cpp.py`) |
| 3, 6 and 10 | 191 |
| 0, 1, 3, 4, 6, 8 and 9 | 192 (the particle types 0x0a, 0x0e, 0x27, 0x49; no dumps: states built from the level) |
| 17 | 260, 261, 271 (and the hero batches 44 .. 55) |
| 18 | 270, 261, 271 (and the hero batches 44 .. 55) |
| 0 .. 18 | 340 (the 17 particle types no level's code spawns, 0x03 .. 0x4c: their updates registered so that none can hang; no dumps: states built from the level) |
| 15, 17 and 18 | 370 (the particle types 0x1f and 0x50: Giant Clank's streaks, Drek's fleet's laser beams) |
| 16 | 371 (Kalebo III's classes on the race's dumps, the racers finished) |
| 18 | 372 (the hero's frame in level 18's water) |
| 0 .. 18 | 400 (the Metal Detector's display: HUD slot 7's draw 0x23a2d0, with its start / update, on every level's own copy; no dumps) |
| 0, 2, 14 and 18 | 401 (level 18's class hooks +0x14, Aridia's camera callback 0x2fa770, the scenes' level 14 effect 0x23e008 / 0x23db30; no dumps) |
| 0, 2, 3, 10 and 13 | 410 (the levels' own builds of the rail search, the cutscenes' level extras and the hit record) |
| 0 .. 18 | 411 (the pause menu's Special page widgets; the levels' copies of two shared widget functions) |
| 0 | 440 (the main loop's pass in game mode 1, a movie playing: the host's frames of it, against 0x245c28 from its update's call to its next pass; #24) |

For a quick check after a small change, level 0's 63, 79, 118 and 141 are the usual minimum.

## Adding a batch

1. Take the next free number and write `tests/oracle/batch_<N>.cpp`. Start it with a header that
   says what it proves (the functions, by address), which levels it runs on, and, if it reads RAM
   dumps, exactly how to record them (the command line, the frame, what is on screen).
2. Write `void batch_<N>()` in `namespace rac1::oracle`. Start it with
   `if (g_only_batch != N) return; g_batch = N;`. For each port, call `check()` with the name, the
   original's address, the number of cases, the kind of return value, a generator that varies the
   state, and the port. The batches around it show the usual shapes: a class update on the level's
   own mobys, a draw callback on a dump, a particle type.
3. Declare it in `ovl_oracle.h` and call it from `main` in `ovl_oracle.cpp`. The CMake picks up
   the file by itself (reconfigure once).
4. When it reads dumps, take their paths from `dump_path("<set>/<file>.bin")`, and skip its checks
   with `skip_check()` and a message naming the set when a dump is missing.
5. Run it until it passes, then run the reference batches.

A batch holds only our own code, addresses and test logic. It never copies bytes, tables or text
from the game: a value a check needs is read from the disc's files or from a dump when the batch
runs.

## The dumps

The RAM dumps live in `assets/dumps/oracle/`, one directory per set, or wherever
`RAC1_ORACLE_DUMPS` points. `assets/` is never committed: a dump is the game's memory, so it holds
the game's code and data. Each developer records them from their own disc. A batch whose dumps are
missing skips those checks and says which set it wanted; it does not fail.

| Set (`assets/dumps/oracle/...`) | Batches | Recorded by |
|---|---|---|
| `vu1_moby/` | 79, 93, 95 | the game with `RAC1_VU1_CAPTURE` (batch 93's header) |
| `vu1_world/` | 104, 108 | the game with `RAC1_VU1_CAPTURE` and `RAC1_VU1_CAPTURE_ALL` (batch 104's header) |
| `vu0_light/` | 125 | the game with `RAC1_VU0_CAPTURE` (batch 125's header) |
| `kerwan_0385/` | 138 | `tools/oracle/kerwan_tour.sh dumps` |
| `rilgar_0387/` | 140 | `tools/oracle/rilgar_tour.sh dumps` |
| `blarg_0390/` | 141 | `tools/oracle/blarg_tour.sh dumps` |
| `umbris_0391/` | 142 | `tools/oracle/umbris_tour.sh dumps` |
| `flight_0406/` | 143 | the command lines in batch 143's header |
| `menu_0408/` | 144 | the command lines in batch 144's header |
| `trespasser_0411/` | 146 | batch 146's header; level 8's pair `tools/oracle/batalia_tour.sh dumps` |
| `drawcb_0413/` | 147 | the command lines in batch 147's header |
| `drawcb_0418/` | 150 | the command lines in batch 150's header |
| `clank_0415/` | 151, 191 | `tools/oracle/blarg_tour.sh clankdumps` |
| `subtitles_0416/` | 149 | `tools/oracle/subtitles_dumps.sh` |
| `batalia_part2/` | 160 | `tools/oracle/batalia_tour.sh dumps` |
| `gaspar_orxon/` | 170, 171 | the command lines in the batches' headers |
| `weapons_dumps/` | 180, 190 | the command lines in batch 180's header |
| `kerwan_heli/`, `kerwan_heli_jump/`, `orxon_clank/` | 191 | the command lines in batch 191's header (also `clank_0415/`) |
| `gemlik_oltanis/` | 222, 223 | `tools/oracle/gemlik_oltanis_tour.sh dumps` |
| `drek_veldin/` | 271 | `tools/oracle/drek_veldin_tour.sh dumps` |
| `levels_15_16/` | 241, 242 | the command lines in the batches' headers |
| `gamebreaking/` | 370, 371, 372 | the command lines in the batches' headers |

A dump is written by a headless run of the game: `RAC1_RAMDUMP=<frame>,<file>` writes the 32 MB of
EE RAM and the 16 KB scratchpad at that frame ([Environment variables](env-vars.md)). The scripts
in `tools/oracle/` (Linux, bash) run the game headless and write their sets into
`assets/dumps/oracle/`. A dump records the state the game reached in *this* port; that is sound
because the code that leads to it is itself proved by the other batches.

A dump can also come from the original game in PCSX2. A savestate (`.p2s`) is a zip archive, and
its `eeMemory.bin` is the 32 MB of EE RAM; followed by the 16 KB of scratchpad, it has the layout
`RAC1_RAMDUMP` writes. This is how the first states were made in the development repository.

## Continuous integration

The oracle cannot run in the public CI. It needs the generated sources and the disc's files, which
are made from a disc nobody may publish, and most batches need RAM dumps, which are game data too.
The public CI only checks that the code compiles without the generated sources
(`RAC1_NO_GENERATED`). The oracle is run by the developer, on their own machine, before a change is
merged, and the results are given with the change.
