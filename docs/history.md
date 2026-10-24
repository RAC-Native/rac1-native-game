# History

How the project got from an empty folder to a game that plays up to Umbris, in plain words. It runs
from the first commit on 19 May 2026 to 10 October 2026, about 1,950 commits in all. It is a story of
a few good ideas, some dead ends, and several times of starting a part over. For where things stand
now, see [Status](status.md); for the reasons behind the big choices, see [Decisions](decisions.md).

Level names in this text are the in-game ones; the table in [Status](status.md#level-names) maps
them to the names the code uses.

## May 2026: finding out how the game works

**19 May: setting up.** The first day installed the tools, unpacked the disc and listed what is on it.
The same day the first small programs read the level containers, the textures and the static models.

**19 to 22 May: a viewer and a lot of notes.** A first program could show a level in a window with a
free-flying camera. Next to it came notes on how the game's engine starts, how its objects (the
"mobys") are stored, how collision data is laid out, and how the game loads a level's own code from
the disc. That last point turned out to matter a great deal later: every level carries its own build
of the engine.

**22 to 24 May: a fast start that went the wrong way.** A "runtime" was built to stand in for the
PlayStation's hardware, and then, in a rush of work with several assistants in parallel, it grew
gameplay: the hero, enemies, weapons, saves and menus. All of it was written to behave like the game
as we understood it, not copied from the game's own code.

**25 May: the hard reset.** Playtests showed that every fix broke something else and that the
result did not look like the game. The decision, and the project's central rule from then on: port
the original game's code function by function, and write nothing that the original does not do. The
invented code was set aside, and later deleted.

## Late May to August: drawing the world, and the long hunt for Ratchet's animation

**25 to 28 May: the world first.** Drawing came before anything else, so that every later port could
be checked on screen. By 26 May the towers and terrain of Veldin were recognisable, drawn by ported
code. A burst of smaller ports followed: weapons, particles, the HUD, sound, saves, many objects, each
with gaps marked as unported rather than filled with guesses.

**28 May: no more emulated graphics for the 3D.** The first renderer replayed the PlayStation's
graphics commands on the PC. For the 3D world it produced exploding vertices and clipping problems, so
it was abandoned: the 3D is drawn natively, and the emulated path is kept only for flat 2D drawing
(menus, HUD). This is the first step toward what later became the native renderer.

**30 May to 10 June: a stand-in Ratchet and a new idea.** Ratchet was shown through a model exported
to a standard 3D file, as a stopgap, while his animation was studied. Every guess at how the
animation data is encoded made the model explode. On 7 June the project chose a different approach: run the game's own
vector-processor code in a small interpreter instead of working out what it computes.

**June to mid-July: looking in the wrong places.** Weeks went into finding where the finished bone
matrices live in memory. They turned out not to exist anywhere permanent. At one point a week of work
had been done on a buffer that was only a fixed template. In mid-July the animation data's encoding
was finally cracked and a decoder written, with poses within a few degrees of the real ones.

**mid-July to mid-August: close, but not right.** Poses were still wrong in places: limbs spiked or
folded, and a playtest failed. Two real discoveries came out of it (how the joints are indexed, and
where the data passes through the processor's scratchpad), and by 13 August the plan was to run the
original code in the interpreter.

**13 August to 29 September: a pause.** No work was recorded for six weeks.

## Late September to early October: a quick result, then a check

**29 September to 1 October: Ratchet walks.** Animation was running through the interpreter, and in a
few days Ratchet could walk, jump, fight, smash crates and collect bolts, with a following camera. It
looked right. The code was labelled "1:1" with the original.

**1 October: the audit, and the oracle.** The audit found that the "1:1" hero was mostly
reconstruction: plausible behaviour with invented numbers, taken from the wrong functions. The cause
was that nothing could check a port against the original. The fix is the second big turning point: a
small interpreter for the PlayStation's main processor now runs the game's own code on a snapshot of
the real game's memory, and a port counts as done only when it gives the same results and the same
memory changes. The first test: the original hero update was run, and Ratchet went from standing to
walking and sped up the way the real game does. The same day, a rule was set: nothing taken from the
disc may be stored in the repository, so everything is generated from the player's own disc.

## October: porting at speed, with proof

**1 to 4 October: the first hundreds of proved functions.** The hero's states and input, the follow
camera, enemies' physics, debris, crates and the first level objects were ported in batches, each one
compared with the original.

**5 October: 2D, menus and moving between planets.** The HUD and menus were drawn through a reduced
copy of the PlayStation's graphics chip. The pause menu, the Help Desk, checkpoints, the lens flare and
the planet map were ported, along with the change from one level to the next, so that Kerwan could be
entered from Veldin for the first time, through a new debug menu.

**5 to 6 October: the game's own start.** The boot sequence now runs as the original does: the intro
movie, the title screen, the main menu, New Game, the memory card screens, and saving and loading
through a virtual memory card.

**5 to 7 October: mobys, and a second renderer to throw away.** First, characters and objects were
drawn by running their original graphics code. A faster GPU-based version of that (a dead end)
was tried and dropped on 6 October in favour of the **native
renderer**, the Modern Rendering Pipeline (MRP). Objects, the world and the sky moved onto it within a
day, proved against the original path, and it became the default on 7 October. It makes widescreen,
full-detail textures and longer draw distances possible.

**6 October: the invented code is deleted.** What was left of the May runtime and the old viewer
(about 45,000 lines) was removed, since the real ports had replaced it.

**6 to 7 October: every level on its own code.** The ports were made relocatable, so each level runs
on its own build of the engine, as the original does, instead of every level borrowing Veldin's. All
the levels started within a day, and this became the default on 7 October.

**7 October: the first playable planet.** Veldin could be played from the opening to the ending movie
and the flight to Novalis. The first weapons followed, and Novalis, Aridia, Kerwan, Eudora, Rilgar,
Blarg and Umbris were started one after another, each with its own objects.

**7 to 8 October: the small differences that cause big hangs.** Two bugs taught that exactness matters
even at the smallest scale. The PlayStation rounds its decimal numbers differently from a PC, which hung
the Vendor on two planets until the game's code ran with the same rounding. And screen fades, which
exist to hide loading, are now played in a single frame by default, with the original available as an
option.

**8 October: Milestone 1 is set.** The goal became the game playable from the start to Umbris. On
that day came the debug tools only behind a command-line flag, support for any gamepad plus keyboard and
mouse, the Graphics options page, the choice between Original and Modern mode at New Game, the ship's
planet menu and the Vendor's screens. The first parts of Batalia, Gaspar, Orxon and Pokitaru were also
started.

**8 to 10 October: finishing the planets.** Each planet of the milestone got a second pass for what
the first left out: Novalis's remaining objects, Aridia's sand sea, Kerwan's train, Eudora, Rilgar's hoverboard
race, Blarg's boss fight and Umbris's swamp. Along the way came real sun shadows, per-pixel lights, loading in
the background, and a proposed second copy of the sound system so music keeps playing through loads.

**10 October: Milestone 1.** The game is complete up to Umbris. The proof now reaches every
level's own hero code as well, which found and fixed one last difference. Known issues remain and are
listed in [Status](status.md).

**12 to 15 October: public repository and the rest of the game.** This repository started on 12
October with Milestone 1 (0.1.0). In the next days came the European PAL disc on its own, Windows
(experimental), the proof tests (the oracle) moved into this repository, Batalia, Gaspar and Orxon
(0.2.0, then 0.2.1 with Windows fixes), and by 15 October every remaining planet, from Pokitaru to the
Veldin finale with the credits and the end menu (0.3.0), together with a large performance update and
an in-game updater. The new planets still await a full playtest.

**16 to 18 October: fixes and the code left to port.** The bug reports on 0.3.0 were worked
through: the crashes on Quartu and Kalebo III, modern aiming, the camera after a strafe, the
widescreen edges, the Vendors' screens and the modern lighting on the later planets. A port inventory
sorted every function of the disc's code into ported, left to port and never reached, and the code
it found left to port was ported, Quit Game back to the title screen among it, until one function
remained (0.4.0). Then the whole game was played through by hand to the end, for the first time on
the port, and the start of a New Game+ checked.

**19 to 20 October: after 0.4.0.** The launcher for Windows and Linux, which installs, updates and
starts the game from one file; the modern lighting reworked around the level's own sun; the sound
driver brought closer to the PS2's, with 5.1 and 7.1 output; an ARM64 job in the CI, not blocking
yet. On 20 October the repository moved from Daino's own GitHub account to the
[RAC-Native](https://github.com/RAC-Native) organization; the old address redirects. Windows,
played through by players, stopped being called experimental.

**20 to 22 October: Vulkan, the wide view and the CPU.** A widescreen HUD (Options > Graphics > HUD),
the Visibomb's view and Aridia's sand sea right across a wide view, the frame-rate cap paced evenly,
the sun rays back to their first look in three steps, AO Intensity, Novalis's neons lighting their
rooms. The game's own update became 12 to 15 % cheaper on the CPU (about 8 % more when built for the
player's own CPU) with its state the same bit for bit, and the debug menu gained a Profiler. On 22
October an experimental Vulkan renderer joined OpenGL, opt-in, drawing the same picture.

**22 to 24 October: 0.5.0.** The generated code was compiled without its debug info (a program less
than half the size), and the documentation gained a catalog of every function and moby class. Then
the reports were worked through: the sun rays no longer lay a veil over the frame with the sun in
view, and none shine in the ship's flights; every gamepad is read, so a virtual one can no longer
hide the real pad; Mirrored Levels stays mirrored while strafing; the Windows setup checks the C
compiler too; the Vulkan pause menu opens at once. Gemlik Base got its own sun, turned to the
skybox's, like Aridia's; the game frame's coroutine got a hand-written context switch for x86-64
and ARM64; the debug menu's Unlocks tab can unlock everything at once. After a playtest on Linux it
was released as 0.5.0 on 24 October, the first release from the RAC-Native organization and the
first with the launcher attached.

## What came of it

- Five months of trying became one rule: nothing is invented, and every port is checked against the
  original.
- The real turning points were the reset to faithful porting (25 May), the native 3D (28 May), the
  oracle (1 October), the per-level code (6 to 7 October), the native renderer (6 to 7 October) and
  the playable planets (7 to 10 October).
- Most of what was built between late May and September has been replaced; what lasted from it is the
  knowledge: the file formats, the notes and the tools.
