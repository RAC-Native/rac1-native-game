# Architectural decisions

The project records its irreversible choices as decision records. This page summarises the
thirteen taken so far: for each, the problem, the decision and what followed from it. Later
decisions sometimes replace earlier ones; the status line of each says where it stands today.
Comments in the source cite them as "ADR 0001" to "ADR 0013": the number is the section
number on this page.

| # | Decision | Status |
|---|---|---|
| 1 | [Non-matching decompilation](#1-non-matching-decompilation) | accepted |
| 2 | [Port function by function, from the original](#2-port-function-by-function-from-the-original) | accepted |
| 3 | [No invented code; render first](#3-no-invented-code-render-first) | accepted |
| 4 | [No GS emulation for the 3D](#4-no-gs-emulation-for-the-3d) | accepted; its moby part replaced by 7, then by 10 |
| 5 | [A VU interpreter for the microprograms not yet ported](#5-a-vu-interpreter-for-the-microprograms-not-yet-ported) | accepted |
| 6 | [An R5900 interpreter as the porting oracle](#6-an-r5900-interpreter-as-the-porting-oracle) | oracle accepted; runtime bridge rejected |
| 7 | [Mobys drawn through their ported VU1 pass](#7-mobys-drawn-through-their-ported-vu1-pass) | accepted; now the reference path, the MRP (10) draws by default |
| 8 | [Every level runs on its own overlay](#8-every-level-runs-on-its-own-overlay) | accepted, the default |
| 9 | [A GPU moby renderer](#9-a-gpu-moby-renderer) | proposed, replaced by 10 |
| 10 | [The Modern Rendering Pipeline](#10-the-modern-rendering-pipeline) | accepted, the default renderer |
| 11 | [The EE's float rounding](#11-the-ees-float-rounding) | accepted |
| 12 | [The game's frame on a coroutine](#12-the-games-frame-on-a-coroutine) | accepted |
| 13 | [The IOP's sound runs twice](#13-the-iops-sound-runs-twice) | **proposed** |

## 1. Non-matching decompilation

**Problem.** A matching decompilation rebuilds source that compiles, with the original toolchain,
to a byte-identical binary. It proves equivalence mechanically, but every line is constrained by
the old compiler, and the effort is an order of magnitude larger. The project's goal is a playable
PC port, not an archival rebuild.

**Decision.** Port-style, non-matching decompilation, as OpenGOAL and the PC ports of Super Mario
64 did: the game's code is rewritten in readable, modern C++20 and checked by its behaviour, not by
comparing binaries. Every ported function carries `// orig: 0xADDR`, the address of the original
it ports, so the original can always be found. Data whose layout is observable (anything in the
game's memory, anything sent by DMA) keeps its original layout.

**Consequence.** Readable code that does not need PS2 toolchain knowledge to follow. There is no
automatic "this file matches" check, so equivalence has to be proved another way: that is
decision 6.

## 2. Port function by function, from the original

**Problem.** Early work wrote game logic (hero movement, rendering dispatch, HUD, weapons) as
reimplementations inspired by the original: code that looked right without an original function
behind it. Each fix opened new bugs, because the invariants being fixed were invented, and the
result did not add up to a recognisable game.

**Decision.** A hard split:

- `runtime/` holds the hardware only (EE memory, GS, VU, IOP, DMA, files, timers) and takes no game
  decision.
- `src/game/` is the port. Every function in it ports one specific function of the original
  executable or of a level overlay, carries its `// orig:` address, and keeps the original's call
  graph. A callee that is not ported yet is called through a `TODO_PORT(0xADDR)` stub.
- Each port must match the original's observable behaviour.

**Consequence.** Fidelity is structural: code in `src/game/` cannot exist without an original
address. Progress became slower per function but stopped going backwards. The invented code was
first quarantined and later deleted.

## 3. No invented code; render first

**Problem.** As long as the invented code was still built and linked, it silently filled the gaps
of incomplete ports, which hid what was really missing and invited more invention.

**Decision.** Three rules:

1. The invented code is no longer linked. A subsystem that is not ported does nothing: the screen
   stays black where rendering is missing, Ratchet stands still where the hero update is missing.
   That is the honest baseline.
2. Port in order: world rendering first (so every later port can be checked on screen), then
   gameplay, then engine systems, then the faithful HUD and menus.
3. Openly invented developer tools (a debug menu, free camera, level select) live in
   `src/_devtools/`, never in `src/game/`, and can be compiled out (`RAC1_DEVTOOLS=OFF` in CMake).

**Consequence.** Missing behaviour is visible and tracked instead of being papered over. The
quarantined invented code has since been deleted entirely.

## 4. No GS emulation for the 3D

**Problem.** The first renderer turned the game's GS register writes back into OpenGL draws. The
GS works on 16-bit screen coordinates after the VUs have projected and clipped the geometry; going
from those packets back to a 3D pipeline produced exploding vertices, needed a software clipper,
and fought with the native drawing.

**Decision.** Follow OpenGOAL: the 3D (tfrag, tie, shrub, mobys, sky) is drawn by native OpenGL
from 3D vertices and matrices, and the GPU does the projection, clipping and depth test. GS
emulation stays only for the 2D: HUD, menus and screen-space effects.

**Consequence.** No vertex explosions, real depth buffering, any resolution. Material state has to
be read from the models' own data instead of from GS packets. Decision 7 later routed mobys back
through their VU1 programs, and decision 10 brought all the 3D back to native drawing, this time
fed by the ported code's own outputs.

## 5. A VU interpreter for the microprograms not yet ported

**Problem.** Much of the game's heavy work runs as microcode on the vector units: VU0 skinning,
VU1 transform and lighting of the world and the mobys. Their data formats are undocumented, and
reverse-engineering what the microcode computes stalled the hero's animation for a long time.

**Decision.** Run the microcode itself. A VU0 and VU1 interpreter (`runtime/vu/`) executes the
programs the game uploads, with the VIF unpacking data into VU memory and the GIF output read back.
It is part of the hardware layer and holds no game logic. It is scaffolding: each microprogram that
is later translated to C++ leaves the interpreter path.

**Consequence.** The animation and the VU-drawn passes worked as soon as the real code ran. The
programs were then translated to C++ instruction by instruction and registered by the hash of their
code (`runtime/vu/vu1.cpp`, `runtime/vu/vu0.cpp`). The interpreter remains as the reference they
are checked against. While playing, a VU0 program without a translation still runs on it; a VU1
program without one is skipped and logged (`RAC1_VU1_INTERP` runs it on the interpreter instead).

## 6. An R5900 interpreter as the porting oracle

**Problem.** Some "ports" were really reconstructions: plausible behaviour, invented constants,
sometimes the wrong original function. The tests compared the code with itself or with hand-picked
numbers. Nothing compared a port with the original.

**Decision.**

- **Accepted: the oracle.** A small R5900 interpreter (`runtime/ee/ee_interpreter.cpp`) runs the
  original function on a RAM snapshot recorded in PCSX2; the port runs on the same snapshot; their
  memory writes and return values must match. The interpreter is not an emulator: no timing, no
  interrupts, no GS, VIF or IOP.
- **Rejected: a runtime bridge.** Letting the game call not-yet-ported original functions through
  the interpreter at runtime was rejected. The goal is readable, modifiable source: game code is
  ported, never executed by the interpreter while playing.

**Consequence.** A port either matches the original on recorded states or it is not done. RAM
snapshots became important project inputs; they are the user's own data and are never committed.
See [Overview](overview.md#the-oracle) and [The oracle](oracle.md): every change to ported game code
ships with an oracle batch and passes it before it is merged.

## 7. Mobys drawn through their ported VU1 pass

**Problem.** Drawing mobys natively from decoded models left out everything the game's own moby
pass computes: the lighting (two directional lights and an ambient per moby), tints, per-moby
alpha, the drop shadows, and every special case of its VU1 programs.

**Decision.** Port the moby pass faithfully: its EE side, checked by the oracle, and its VU1
programs, checked against the VU interpreter. The host draws the GS packets they produce with its
GS replica.

**Consequence.** Shadows, lighting, tints and alpha came from the game's own code, but drawing
through the GS replica was slow. This faithful path is kept as the reference that the MRP
(decision 10) is proved against, and can still be selected with the debug tools (see
[Rendering](rendering.md#the-original-rendering-kept-as-the-reference)).

## 8. Every level runs on its own overlay

**Problem.** Each level of the game ships its own overlay: the whole engine relinked at other
addresses, plus that level's own classes and data. Running every level on Veldin's overlay image
played the levels, but the level-only classes had no code (they stood inert), their data had no
home, and the per-level variants of engine functions ran Veldin's version.

**Decision.**

1. The host loads the played level's real overlay.
2. The ports are written against Veldin's addresses but made relocatable: every overlay address in
   `src/game` goes through `OV(0x...)` (data), `OVF(0x...)` (code) or `OvConst<0x...>`
   (`src/game/ovl/ovl_reloc.h`). On Veldin this is the identity; on another level a per-level table
   generated from the user's ISO gives the address there.
3. Class update functions, camera types and sound handlers are found through the level's own
   tables, and translated back to Veldin's address to find their port (`ovl_code_to_veldin`).
4. Level-only code is ported at its own addresses, with level-tagged annotations such as
   `// orig: L1:0x...`.

**Consequence.** Veldin is unchanged by construction, and each level can be ported in full with
its own classes and data. Running every level on its own overlay is the default;
`RAC1_OWN_OVERLAY=0` runs them all on Veldin's image for comparison. See
[Architecture](architecture.md#per-level-overlays-and-the-ov-macros).

## 9. A GPU moby renderer

**Problem.** Running the translated VU1 programs on the CPU and drawing their output through the
GS replica cost tens of milliseconds a frame, and the GS depth mapped onto the host's depth buffer
let the world show through characters where they touched it.

**Decision proposed.** A GPU renderer for mobys fed by the same inputs as the faithful pass, with
shaders reproducing the VU1 math and the GS fragment rules.

**Consequence.** It worked as a first step but still ran inside the frame's packet stream and used
its own projection. It was replaced by decision 10, which reused its GS-state shader.

## 10. The Modern Rendering Pipeline

**Problem.** Measured on Veldin, the cost of a frame was not the game logic but the host's way of
drawing: whole levels drawn every frame, one draw per GS primitive, textures limited to what the
game kept in its small video memory, and separate projections for the world and the mobys.

**Decision.** One native renderer for the 3D, built the way OpenGOAL's is:

1. **Assets generated offline from the user's ISO** (mobys, tfrag, tie, shrub) and proved exact
   against the data the original sends to the VUs. They are loaded once per level into GPU buffers
   and texture arrays with every mip level.
2. **Per frame, only what the ported code produces:** bone matrices, lights, tints, alpha, the
   visibility the game's culling decided, the camera.
3. **Shaders reproduce the VU1 programs' math and the GS state**, with constants from the ported VU
   code, never tuned.
4. **One projection** for the world, the mobys and the sky.
5. **Proof:** the faithful VU path stays as the reference, and the MRP is compared against it.
6. **Draws grouped** with multi-draw-indirect: tens of draw calls a frame instead of thousands.

**Consequence.** The MRP has been the default renderer since October 2026; the faithful path can be
selected with the debug tools. The faithful VU path and the GS replica remain for the 2D and as the reference. See
[Rendering](rendering.md).

## 11. The EE's float rounding

**Problem.** The EE's FPU and the VUs round additions, subtractions and multiplications toward zero
and flush denormals to zero; PCSX2 runs them that way. The port first rounded to nearest, as a PC
does, and so did the oracle's interpreter, so the oracle compared two round-to-nearest machines and
could not see the difference. It showed up as a hang: a Vendor's opening animation adds 0.1 while a
value is below 1 and its closing takes 0.1 off while it is above 0. Rounding toward zero, these end
at 1.0999998 and then about -4.2e-7, which is harmless. Rounding to nearest, they end at 1.0000001
and then -0.1: the Vendor's scale went negative, its collision rectangle turned inside out, and
`moby_grid_move` (0x250b20) looped forever.

**Decision.** The thread that runs the game's code switches the host FPU to round toward zero with
flush-to-zero and denormals-are-zero (`ps2fpu::enter_ee_rounding()`, `runtime/ee/ps2_fpu.h`).
Plain C++ float arithmetic in a port then rounds as the EE does. The EE's divide and square-root
unit rounds to nearest, so an original `div.s` is written `fpu_div(a, b)` in a port
(`src/game/ovl/ovl_common.h`, built on `ps2fpu::div_nearest`): it takes the truncated result and
steps to the next float when the exact value is past the midpoint, without switching the FPU mode (switching
it cost a third of the frame's update). A VU0 divide rounds toward zero, so a plain `/` is right
there. Everything is compiled with `-frounding-math`. `RAC1_FPU_NEAREST=1` brings back
round-to-nearest, for comparison only.

**Consequence.** The hang disappeared, and positions and angles now match the PS2's to the last
bit, which changed the faithful renderer's reference images once.

## 12. The game's frame on a coroutine

**Problem.** The game's main loop runs one frame per pass, and the host runs one pass per host
frame, presenting a picture in between. But some of the game's code runs several frames inside a
single call and waits for the vsync in the middle: the screen fade (n + 2 vsync waits), the level
exit's cards and movies, the boot module's memory card screens and title wait. On the PS2 the
caller simply blocks there while the frames go by. Cutting each such loop by hand into a state
machine resumed by the host would mean splitting every caller; the fade alone has 33 call sites on
Veldin, some in machine-translated files that cannot be split.

**Decision.** The game's frame runs on a coroutine of the host thread (`runtime/ee/ee_thread.cpp`:
a hand-written context switch on Linux (x86-64 and AArch64; `ucontext` on other architectures), a fiber on Windows, through `src/platform/`, with its own 64 MB stack). The
host calls `ee_thread_run` for one game frame. A vsync wait inside a blocking loop calls
`ee_thread_vsync`, which suspends the coroutine: the host frame ends there and presents what the PS2 would show after that vsync, and the next host frame resumes
the game right after the wait. The port of a blocking loop is the loop as written. The FPU mode of
decision 11 is carried into the coroutine.

The screen fade existed to hide the PS2's disc loading. By default the port runs its loop whole,
with its memory writes and the frame counter moving by n + 2 as on the PS2, but without showing its
frames, so the screen cuts. The original fade, its frames shown, is the Advanced option
"Fades: Original".

**Consequence.** Blocking loops are ported as written, and the fade is faithful in its state.
Outside the coroutine (for example in the oracle) the waits return at once and the loop runs to its
end inside the call, with the same memory writes.

## 13. The IOP's sound runs twice

**Status: proposed.** It is implemented and on by default, but has not been accepted yet.
`RAC1_IOP_MIRROR=0` turns it off.

**Problem.** On the PS2 the sound runs on the IOP, a processor of its own: when the EE is busy for
a long frame (reloading a checkpoint, starting a level, opening a movie), the IOP keeps playing the
music and the sounds already started. In the port, the IOP's sound driver runs on the game's thread
and advances 800 samples per vsync the game waits for, which keeps everything the game reads back
from the sound (is this sound still playing, how far into its stream) a function of the game's
vsyncs only. But when the game's thread stalls for longer than the audio queue, the sound stops.
Advancing the sound on the host's clock instead would make the game's state depend on the host's
speed.

**Decision.** Two copies of the 989snd driver run (`runtime/iop/snd989_iop.cpp`):

- **The game's IOP**, as before, on the game's thread. Its replies and state are what the game
  reads. Its output is no longer played.
- **The host's mirror**, on a separate audio thread. The game's IOP records every command, bank
  load and vsync it receives; the mirror replays them and its output is what you hear. While the
  game keeps up, the mirror produces the same samples. When the game's thread stalls, the mirror
  goes on alone, so the music continues.

**Consequence.** No sound gap during loads, and the game's state is unchanged. After a stall, the
mirror is ahead of the game by the stall's length, as a PS2's IOP would be after a long EE frame.
The sound driver's CPU cost is paid twice. See [Audio](audio.md).
