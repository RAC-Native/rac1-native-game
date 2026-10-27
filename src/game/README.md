# `src/game/` — faithful function-by-function port of R&C1

Per [ADR 0002](../../docs/decisions.md), every source file under this tree is a port of specific
original functions from the R&C1 ELF (`SCUS_971.99`) or a level overlay (`<level>.rax`), except
`render/` and `enhance/`: host code, whose functions are marked `// orig: (host)` or `// orig: (none)`.

## The contract

Every C++ function in `src/game/` must:

1. Carry a `// orig: 0x<address>` comment within 5 lines above its definition, citing the
   function's address in the original ELF (or overlay). The comment may add the original Insomniac
   name where it is known.
2. Be a 1:1 behavioural port of *that* address. Not "inspired by", not "similar to". When the
   original branches, this branches. When the original mutates a global at `0x...`, this does too.
3. Use modern, readable C++20 (per [ADR 0001](../../docs/decisions.md)): codegen is not matched,
   behaviour is.
4. Be proved against the original: the original function is run in an EE interpreter and the port
   must produce the same memory writes and return value ([ADR 0006](../../docs/decisions.md)). The
   differential tests that do this are the oracle, in `tests/oracle/` (see [The oracle](../../docs/oracle.md)).

## What to do when the function calls something not yet ported

Use the `TODO_PORT(addr, name)` macro from `src/game/port_stubs.h`. It logs the unported call site
once and returns a typed safe default.

## Layout

The ports run the game's own code paths, grouped by where the original code lives:

```
src/game/
  ovl/           # the level overlay's code (Veldin's build, which matches in every level's
                 # overlay) — game frame, mobys, hero, camera, HUD, menus, sound, the draw passes
  lib/           # base-ELF libraries (libc, libgraph, libvu0, libcdvd, 989snd client)
  math/          # base-ELF vector math
  boot/          # the boot module and its disc reads
  input/         # the controller packet
  render/        # the host's renderer: the Modern Rendering Pipeline (OpenGL and Vulkan) with its
                 # lighting, materials, LOD, effects and particles, plus the older tfrag / tie /
                 # shrub / sky renderers (ADR 0004) and moby draw (ADR 0007) kept for comparison
  enhance/       # the PC options pages (Graphics, Controls, Advanced) and the update notice
  port_stubs.h   # the TODO_PORT macro
  debug_hooks.h  # hooks for the host debug menu (src/_devtools)
```

## Adding a new port

1. Find the function in Ghidra and note its address.
2. Implement the C++ port and annotate it with `// orig: 0x...`.
3. Replace any `TODO_PORT` stub that stood in for it with the real call.
