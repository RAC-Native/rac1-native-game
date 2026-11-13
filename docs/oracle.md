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
| `ORACLE_WRITES=1` | After each check, a `WRITES` line: in how many cases the original changed nothing (a check that compares nothing), how many words it changed, whether it wrote into the null page and how far it moved the packet cursor `0x1611c0`. Run it on a new check before trusting it. |

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
| 0 and 3 (its dumps); 0 .. 18 (its level-built checks) | 180 (the shared weapon classes: the drones, the chicken and its debris, classes 804 and 166, the Morph-o-Ray's HUD element, particle types 5 and 0x37) |
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
| 0, 3, 5, 6, 11, 12 and 18 | 480 (the world passes' culls written by hand: 0x294368, 0x295fa0, 0x286f10, on the `world_cull/` dumps) |
| 0, 3, 5, 6, 11, 12 and 18 | 481 (the tfrag lighting written by hand, 0x295350, on the same dumps; batch 101 checks it on Veldin and the title) |
| 0, 3, 5, 6, 11, 12 and 18 | 482 (the drop shadows' VU0 routines written by hand, through shadow_points 0x285a68 and drop_shadows_draw 0x285870 on the frame's deferred mobys, on the same dumps; batch 79 checks them on Veldin's mobys) |
| 0, 3, 5, 6, 11, 12 and 18 | 483 (the sky's routines written by hand, 0x28c14c, 0x28c3c0, 0x28c298, 0x28c680 and 0x28bd58, on the same dumps) |
| 1 .. 18 | 501 (stage 4's generalised hand ports of Novalis's and the level-1 hero's helpers, each against every level's own copy at its own address: the water grids and the strips' packet (`novalis_water.cpp`), the wall-path helpers and particle 0x34 (`novalis_amoeboid.cpp`), class 805 the spawn point, the scripted camera's nodes, the plants' bolts, the moby clone and on-screen test (`novalis_mobys.cpp`), the groups, the walker record, the cooldown slot and the path's next point (`novalis_enemies.cpp`, `novalis_hive.cpp`), the level-data note, the Plumber's nodes and the hero's walk (`novalis_npcs.cpp`), the path follower, the nearest point and the debug particle (`novalis_path.cpp`), the special mode's exit and idle (levels 1 and 2); no dumps: states built from the level; a check prints "not on level N" on a level without its copy) |
| 1 .. 18 | 502 (stage 4's generalised ports, one level's hand port made to serve every level's copy, each against every level's own copy at its own address: camera type 3 (1, 5, 8, 16, 17), camera type 1 (3, 4), the follow camera's look-height hook, its callback and the callback queue (2 .. 18 but 8), the follow camera's distance rates (2, 3, 6, 7, 8, 12, 15, 18), the turn toward a direction (3, 4), class 605's zone marker (1 .. 18), the teleporter's pieces (1, 7, 8, 11, 12, 13, 16, 17, 18) and the moby group's state (1, 5 .. 8, 11, 12, 13, 15 .. 18), the Peckbot's and the Amoeboid's carry hooks (1, 11, 13), whether the hero is on a moby (1, 3, 7, 9, 10, 12, 13, 14), class 915 (3, 10); no dumps: states built from the level; a check prints "not on level" on a level without its copy) |
| 0, 2, 3, 5, 7, 8, 9, 10, 12, 14, 15, 16 and 18 | 503 (stage 4's generalised hand ports of task G3, each against every level's own copy at its own address: the ship's shadow, engine glow and exhaust trails (`aridia_ship.cpp`, every level's copy of Veldin's 0x28e670 / 0x28efc8 / 0x28f238) and the packet's VIF tag `dma_cnt_vif` (every level), the background ships' glows (2, 9, 10), class 331 (2, 8, 18), the escalator (2, 12), the draw-hook appenders (2, 5, 7, 9, 12, 14, 15), the ocean's wave table (2, 3, 5, 7, 8, 9, 14), and the hoverboard races' camera `shared_race_camera.cpp` (5, 16); no dumps: states built from the level, the draws' packets at a cursor on a test-area buffer (`B503_INFO=1` prints how far they grew); a check prints "not on level N" on a level without its copy) |
| 5, 9, 11, 13, 16 and 17 | 504 (stage 4's shared class ports of task F1, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 1886, the blob class 1885 throws, its mark and retirement (9, 13), the point ahead along a path (9, 13), class 1111's open sea and its draw (11, 16; "class1111_fix24" checks its 1.02 Gameplay fork with RAC1_GAMEPLAY102=24), the hoverboard races' trick meter and bar (5, 16), the space levels' pickups 224 / 228 and rocket 295 (13, 17), the space combat's shots 1009 / 1017, the rocket 1034, their launchers and the homing shot's target search (11, 13, 17), class 1319 / 1843's trails, path pose and hit (11, 17), the HUD's line strips, turned quads, ring and screen point (11, 13, 17), a group's flags (13, 16, 17), a spawn at a place and an eased value (13, 17), the state tests (11, 16), the draw-queuing classes 1952 / 1907 .. 1918 (16, 17); the draws' packets at a cursor on a test-area buffer (`B504_INFO=1` prints the subjects and how far the packets grew); no dumps: states built from the level; a family the level does not carry says so) |
| 15, 16, 17 and 18 | 505 (stage 4's shared class ports of task F2, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 49's shot, its trail (also class 50's copies, with their own record; 15 .. 18), its shards and spawn (16, 18), class 50's spawn (16, 18), class 256's homing rocket (15, 18), class 638's gun drone (update, hits, seek, turn, burst, aim, sparks, group timers; 16, 18), class 1356's carrier (update, hits, path following, the paths' headings, its joint, a drone attached and let go; 16, 18), class 1523, Giant Clank's charged shot, and its streak (15, 18), class 123's laser emitter (15, 17), class 1443 / 1890's rings (update, group helpers, draw; 16, 18); the draw placed where its quads reach the packets; no dumps: states built from the level; a family the level does not carry says so; the downed carrier's debris' spin w lane is left out of class 1356's update check) |
| 0 .. 18 | 506 (class code several levels carry a copy of, ported once: `src/game/ovl/shared_*.cpp` (classes 455 / 545, 1633 and the Sandmouse's glow, 567, 679, 258, 341, 1135's and 252's draws, the bosses' health bar, camera type 19, the rocks, the height map's lookup and puffs, small helpers), each against every level's own copy at its own address; no dumps: states built from the level, the screen and projection set up and the draws' packets on a test buffer (`B506_INFO=1` prints, per check, the cases that drew beyond a state header and the cameras' turns) |
| 1, 3, 7, 14, 15 and 17 | 507 (stage 4's shared class ports of task F4, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 1224's bolt of lightning (update, bolt, strike, fade, draw; 14, 15, 17), class 403's rocket and trail (14, 15), class 325's shell (14, 15) and its glow (also Novalis's class 1510's; 1, 14, 15), classes 250 and 1331 (14, 15), class 28's two draws (14, 15), class 77's mine and its group's glow (15, 17), the draws of classes 78 and 123 and the three glows of classes 44 / 1382 (15, 17), the rippling surfaces' update and two draws (15, 17), classes 196 / 197, class 655's glows, class 408's tint (15, 17), class 848's gunfire and its bullet (3, 7, 15), a spark at a chain point (7, 15, 17), the held moby shown (1, 14), class 686's shell (1, 14); the draws placed where their corners reach the packets; no dumps: states built from the level; a family the level does not carry says so) |
| 0, 2 .. 8 and 10 .. 17 | 508 (stage 4's shared ports of task F5, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: classes 1179, 933 (and its carriers' helpers), 235, 439 and the hoverboard race's boost pads and HUD element, the debris pieces (344 .. / 980), the path-riding rafts' helpers, 823 and its draw, 1511's draw, 1221 / 1222, 1371 (update, spawn, throw), the panels' lights (two builds), the ripple meshes' draws (Quartu's, Drek's fleet's), Blarg's mesh draws (classes 1062 / 1083), Oltanis's arrival sparks and glow (with Veldin's copies, which `cutscene.cpp` now runs), the particle types 0x50 (Veldin's registration now runs the port), 0x43, 0x3a and 0x28 spawns, and small helpers; no dumps: states built from the level; a check prints "not on level N" on a level without its copy; the draws' packets go to a test-area buffer and each draw check prints how many of its cases drew) |
| 1 .. 18 | 509 (stage 4's shared class ports of task F6, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 827 (a walker with a rider, its helpers and class hooks), classes 302 / 303, the draws of classes 1302, 857 and 1108, the swingshot's targets (classes 758 / 803) with the camera's look toward them and its hook's install, class 819 (a homing shot), class 1021 (a flower of petals), class 832 (the Visibomb's range and its static), class 604, particle 0x4a's spawn, the path links, class 830 (a floor switch), classes 1038 / 1040 (a crystal and its holder), class 1378 and its draw; no dumps: states built from the level, the screen and projection set up and the draws' packets on a test buffer (`B509_INFO=1` prints each draw case's packet growth); a check prints "not on level" on a level without its copy) |
| 0, 1, 2, 4 .. 13, 15, 16 and 18 | 510 (stage 4's shared class ports of task F7, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: the background ships, movers, puffer, homing missile, shells and shots, tracer, flickering light, Giant Clank's pad (and its 1.02 fork: `clank_pad_fix23`, run with `RAC1_GAMEPLAY102=23` on 15 and 18), flame spray, environment-mapped objects, beams, arcs, tile grids, scrolling textures, deadly zones, carried mobys, shock wave, the finale's quads and hooks, the flash, small helpers and spawns, the particle types 0x21 / 0x31 / 0x3f; no dumps: states built from the level; a check prints "skipped" on a level without its copy; the gold weapons' scene start is not compared: 989snd's SIF; the screen and projection set up, `B510_INFO=1` prints, per check, the cases that drew beyond a state header) |
| 1, 5, 7, 11, 12 and 13 | 511 (stage 4's generalised hand ports of task G4, one level's hand port made to serve every level's copy, each against every level's own copy at its own address: the Amoeboids, classes 572 / 865 / 866 (their update through every state, the frame before the state, the split, the class-0x26f target search, the splash and the drips; `novalis_amoeboid.cpp`; 1, 5, 11), the water (the VU1 set-up, the grid's light and positions, the uv scroll, a cell's packet, a cell, the tiles' draw and the class's draw callback; `novalis_water.cpp`; 1, 5, 7, 11, 12, 13); no dumps: states built from the level (group lists of the test area, the hero placed at the target, next to a wall path's edge, in the box), the screen and projection set up, the draws' packets at a cursor on a test-area buffer (`B511_INFO=1` prints the subjects and how far each case's packets grew); a family the level does not carry says so) |
| 1, 2, 3, 4, 6, 7, 8, 10, 12, 13, 14 and 18 | 512 (stage 4's generalised hand ports of task G5, one level's hand port made to serve every level's copy, each against every level's own copy at its own address: camera type 18 (0x12; its accept, frame, cone and zone tests) and the follow camera's word +0x218 (`kerwan_camera.cpp`; 2, 3, 6, 7, 8, 12, 18), the path follower's point, index wrap and advance, spline tangent, Hermite curve and length (`novalis_path.cpp`; 1, 3, 4, 14), the aircraft base (Kerwan's class 845 and its copies), its bank (and Kerwan's second build of it, class 75's) and its smoke particle (`novalis_jet_base.cpp`; 1, 3, 4, 14), class 737's scripted-camera trigger (`novalis_mobys.cpp`; 1, 2, 3, 7, 10, 13); no dumps: states built from the level, the camera's zone shapes and data in the test area; a family the level does not carry says so) |
| 1, 2, 3, 5, 7, 8, 9, 10, 12, 13, 14, 16, 17 and 18 | 513 (stage 4's generalised hand ports of task G6, one level's hand port made to serve every level's copy, each against every level's own copy at its own address: class 1135, the teleporter pad (its update through every state, trigger, hint, hide, show; `novalis_teleporter.cpp`; 1, 7, 8, 12, 13, 16, 17, 18), the lift (class 726 / 1141; `novalis_platforms.cpp`; 1, 3, 10, 13), the gold weapon case (`novalis_cases.cpp`; 1, 13; the scene start and state 4's level change not compared: the IOP), the ocean's set-up, transfer, cells' test and draw, texture blend, end and one-record draw (`aridia_ocean.cpp`; 2, 3, 5, 7, 8, 9, 14); no dumps: states built from the level, the screen and projection set up, the draws' packets at a cursor on a test-area buffer (`B513_INFO=1` prints the subjects and how far each case's packets grew); a family the level does not carry says so) |
| 1 .. 18 | 514 (stage 4's shared class ports of task F8, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 28, a path-riding turret, and its sixteen helpers (14, 15), class 193, a creature that charges and strikes (9, 15), class 156, a shot that bursts on what it meets, its burst and trail (1, 6, 8, 9, 15, 17), class 1134, the gold bolt, its glitter, camera, sparkles and spin and the glitter's pvars (both builds; 1 .. 18), class 1818, the Sandmouse, its step, target search, shot, shot spawn (class 1633), sparks, goals and glow points, and the point ahead of the hero (all its copies; 1 .. 6, 8, 11, 12, 14); no dumps: states built from the level, the frame scale at its 50 Hz value in a quarter of the cases; the console clock and disc reads of the gold bolt's level change are the host's on both sides; the segment records 0x173e90 .. 0x173ebc are left out of the checks that test lines against the world, and the w lanes enemy_physics stores out of class 28's update; a check prints "not on level N" on a level without its copy) |
| 1 .. 9, 11, 12, 13 and 18 | 515 (stage 4's shared class ports of task F9, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 615, the Trespasser's pad (its update, the camera's goal, the hero pulled onto it) and its puzzle's draw callback with the link, ring, marker and frame draws (2, 4, 6, 8, 11, 13, 18), class 1844's breakable prop (4, 8, 9, 11, 12), the ocean's wave table with the alpha 0x80 (`aridia_props.cpp`; 5, 7, 8), class 447 / 920 (3, 5, 7, 13), class 1023's walker, its every-frame part and its shot's spawn (8, 9), class 280's pump with Novalis's story flags and the scripted camera's angles (1, 4, 8), class 631's flying craft and its hits (3, 7); no dumps: states built from the level after its first frame, the draws' packets into a buffer of the test area (B515_INFO=1 counts the cases that drew); a family the level does not carry says so) |
| 15 and 17 | 516 (stage 4's shared class ports of task F10, `src/game/ovl/shared_*.cpp`, each against each level's own copy at its own address: class 44, a guard (its update through every state, its frame's common part, its look, its ally search, turns, walk, its group's alert and calm, its path order, its look at another guard), class 408, the guards' alarm (its update, the frame's alarm, its raise and wake), class 78, a turret (its update, wake and break) with its sparks (particle 0x47) and the guards' shot (class 934), class 655, an electric field (its update, its arc and the arc's reset, the hero's ground test, class 1179's lift); states built from the level (its paths, alarms, groups, boxes), no dumps; `B516_INFO=1` prints the subjects found) |
| 2, 5, 7 .. 10, 12 .. 18 | 517 (stage 4's shared class ports of task F11, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 252's update, move and hits (8, 14), class 52's update and hits (9, 16), class 1400, the weather about the camera, its wind and its two particles (8, 12, 14), class 1885, the blob thrower, its hits, free slot, throw and two random helpers (9, 13; "class1885_fix25" checks its 1.02 fork, 1.02 Gameplay fix 25, with RAC1_GAMEPLAY102=25 on 13), class 1511's update and remains (5, 14), the rafts' next stage, each level's own arrival and Oltanis's checked find (5, 14, 16), the sea of classes 460 / 317 (7, 9), class 1139 (5, 16), the puff of particle 0x34 (10, 12, 13), class 50's update (16, 18), class 886, a checkpoint pad (7, 12), ship 1, class 1212 (2, 9), and the draws: the meshes' callbacks of classes 1952 / 1907 .. 1918 with the second build of envmap_uvs4 (16, 17), class 655's callback, its strips of quads and the finale's glows (15, 17, 18), Hoven's and Oltanis's sky-reflecting objects: their callback, draw and VU0 mapping (12, 14), each draw with the packet cursor on a test-area buffer (`B517_INFO=1` prints how many cases wrote packets beyond a state header); no dumps: states built from the level, the frame scale at its 50 Hz value in a quarter of the cases; the debris' w lanes (debris_spawn's port) are left out of the checks of updates that spawn debris, and the segment record 0x173e90 .. 0x173ebc out of class 50's; class 252's carry record is left as the level has it (carry_update's port differs in random records); a check prints "not on level N" on a level without its copy; B517_INFO=1 prints the subjects) |
| 6 and 10 | 518 (stage 4's shared class ports of task F12, `src/game/ovl/shared_*.cpp`, each against each level's own copy at its own address: class 857, the Gadgebot (its update through every state and sub-state, the deal of targets, its arrival at slots and entrances, its destination's pick, its search, its score, its hit reaction, its glow), the lights it spawns (classes 302 / 303), class 302's update and its break into shards, the shards' spawn, class 1068 (a robot with an electric beam: its update, its frame's common part, the beam's start and run), class 1302, the Gadgebots' entrance (its update and the opening of its mobys); states built from the level, Blarg's Gadgebots also from the Clank-section dumps `clank_0415/` (skipped without them); `B518_INFO=1` prints the subjects found) |
| 3, 6, 7, 8, 11, 14, 15, 17 and 18 | 519 (stage 4's shared class ports of task F13, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 1209, a floor switch of a group (Quartu, Drek's fleet; its update through every state, the hero on it or not, done before or not), class 934, a spinning shot (aimed at mobys, the hero or the open), the breakable props of Blarg and Umbris (classes 1655, 1660, 1664, 1650, 1663: hit or not, breaking), the exhaust of the ships the hero rides (classes 1242 and 1379), Blarg's hydro pumps' camera (class 1051: its start and follow, every copy), the flash's draw (class 1898; its quads reach the test area's packets), three more sets of the scrolling textures' meshes (Batalia, Pokitaru), the sea's controller (Kerwan's class 994, Oltanis's 1260); no dumps; `B519_INFO=1` prints the subjects found and the draws' cases that wrote packets) |
| 1, 2, 3, 4, 6, 7, 8, 14, 15 and 17 | 520 (stage 4's shared class ports of task F14, `src/game/ovl/shared_*.cpp`, each against every level's own copy at its own address: class 437, an older build of the Sandmouse (its update through every state, its step, target search, shot, goal by the hero, fall home, and the moby's collision point; 1, 2, 4), the gadgets picked up before a panel (Aridia's class 1005, the Trespasser, Blarg's class 1016; Quartu's and Drek's fleet's class 1428; the class number dealt so both items' ways run on both levels), class 1629's shower of drops (8, 14), Batalia's two draws of three layers of scrolling meshes and their twelfth mesh set (8), the sea's draw callback (3, 7 (two), 8, 14); no dumps: states built from the level, the screen and projection set up, the frame scale at its 50 Hz value in a quarter of the cases, the stream system up on the levels whose gadgets start scenes, the console clock and disc reads of the level changes the host's on both sides (batch 514's), a zone box of the test area for the hero taken; the segment records 0x173e90 .. 0x173ebc are left out of class 437's checks that test lines against the world; `B520_INFO=1` prints the subjects found and the draws' cases that wrote packets; a check prints "skipped" on a level without its copy) |
| 0 .. 18 | 530 (#145: existing engine ports on each level's own state, where the stage-4 family tasks met differences: segment_collide 0x1efc70 with lines across the level (to mobys, to mobys moved in RAM only, to points up to 200 away; flags 0 / 2 / 0x10); carry_update 0x2dc9f0 and class_hook_10 0x2db890 on the level's carriable mobys and a Morph-o-Ray chicken (the record through its states, the countdown and the class's hook +0xc); no dumps; on Kalebo III its words go to a free moby slot, the level's moby table covering the test area) |

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
| `clank_0415/` | 151, 191, 518 | `tools/oracle/blarg_tour.sh clankdumps` |
| `subtitles_0416/` | 149 | `tools/oracle/subtitles_dumps.sh` |
| `batalia_part2/` | 160 | `tools/oracle/batalia_tour.sh dumps` |
| `gaspar_orxon/` | 170, 171 | the command lines in the batches' headers |
| `weapons_dumps/` | 180, 190 | the command lines in batch 180's header |
| `kerwan_heli/`, `kerwan_heli_jump/`, `orxon_clank/` | 191 | the command lines in batch 191's header (also `clank_0415/`) |
| `gemlik_oltanis/` | 222, 223 | `tools/oracle/gemlik_oltanis_tour.sh dumps` |
| `drek_veldin/` | 271 | `tools/oracle/drek_veldin_tour.sh dumps` |
| `levels_15_16/` | 241, 242 | the command lines in the batches' headers |
| `gamebreaking/` | 370, 371, 372 | the command lines in the batches' headers |
| `world_cull/` | 480, 481, 482, 483 | the command lines in batch 480's header (levels 0, 3, 5, 6, 11, 12 and 18) |

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
