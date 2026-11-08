/* rac1-native's mod SDK: the C interface between the game and a mod's code (a native plugin).
 *
 * docs/custom-levels.md, "Code" says how to write, build and test one. In short: a mod folder with a `plugin/` folder
 * (a CMakeLists.txt that includes this SDK's rac1_mod.cmake, and your C or C++ sources) is built by the setup on the
 * player's machine, from the source, into assets/generated/mods/<folder>/plugin/. The game loads it at its start when the
 * mod is on (a mod with code is off until the player turns it on and confirms it in Options > Mods) and calls the one
 * function it must export:
 *
 *     RAC1_MOD_EXPORT int rac1_mod_main(const rac1_api* api, rac1_mod* mod);
 *
 * which fills `mod` with its callbacks and returns 0 (anything else: the plugin is not used, the log says so). The game
 * also asks RAC1_MOD_EXPORT unsigned rac1_mod_abi(void) (RAC1_MOD_DECLARE_ABI() writes it): the ABI version the plugin was
 * built with; a plugin newer than the game is refused.
 *
 * Versioning: both tables only ever grow at their end (append-only). `api->size` is the game's table's size: before
 * calling a service added after version 1, check RAC1_API_HAS(api, field). `mod->size` is the plugin's: the game calls
 * only the callbacks that fit in it.
 *
 * Addresses: the game's memory is the PS2's (EE RAM, 32 MB); its addresses are 32-bit numbers (uint32_t), Veldin's as the
 * game's code knows them (docs/custom-levels.md lists the useful ones). Data of the current level's overlay moves with the
 * level: api->ov(addr) gives where Veldin's address `addr` is now. Read memory inline through api->ram (RAC1_LW & co.
 * below); write it only through api->sw / sh / sb / swf: the game's renderer runs some of the game's code a second time
 * for a frame inside a journal of the memory and puts the memory back after, and the writes must go through it.
 *
 * The rules:
 * - Callbacks run on the game's thread, one at a time, with the PS2's float rounding (toward zero): the SDK's CMake file
 *   compiles with the game's float rules (no fused multiply-add, no fast math).
 * - A callback may run while the renderer replays a frame (api->replaying() is then 1: the HUD callback does, every frame
 *   with some options). There, write nothing of your own (your variables): the game's memory is put back after it, yours
 *   would not be. The game does not call frame, camera, object_update or the level callbacks while replaying.
 * - Nothing here may be called from another thread, or after shutdown.
 */
#ifndef RAC1_MOD_H
#define RAC1_MOD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The ABI this header describes. 1: the first. */
#define RAC1_MOD_ABI 1

#if defined(_WIN32)
#define RAC1_MOD_EXPORT __declspec(dllexport)
#else
#define RAC1_MOD_EXPORT __attribute__((visibility("default")))
#endif

/* A level as level_start gives it */
typedef struct rac1_level_info {
    int32_t level;         /* the game's level number (0..18): a level mod's level is its base level's (0, Veldin) */
    int32_t slot;          /* the host's level: the same as `level` for the game's, 19.. for a level mod's */
    const char* level_id;  /* a level mod's level.id, "" for the game's levels */
    int32_t mine;          /* 1: the level of this plugin's own mod */
    int32_t objects;       /* (mine) the placed objects (level.txt's moby lines): their uids are 1..objects (the hero's
                              uid skipped) */
} rac1_level_info;

/* The game's services. Append-only: new ones go at the end. */
typedef struct rac1_api {
    uint32_t abi;    /* the game's RAC1_MOD_ABI */
    uint32_t size;   /* sizeof the game's table */
    const uint8_t* ram;  /* the EE RAM (32 MB, the game's addresses 0..0x2000000): read it, never write through it */

    /* -- the log, the mod's files -- */
    void (*log)(const char* text);  /* a line in the game's log, "[mod <folder>] text" */
    /* the full path of `name` in the mod's own folder (into `out`, `cap` bytes); 0 if it does not fit */
    int (*mod_path)(const char* name, char* out, size_t cap);

    /* -- memory -- */
    uint32_t (*ov)(uint32_t veldin_addr);  /* where Veldin's overlay address is on the current level */
    void (*sw)(uint32_t addr, uint32_t v);
    void (*sh)(uint32_t addr, uint32_t v);
    void (*sb)(uint32_t addr, uint32_t v);
    void (*swf)(uint32_t addr, float v);
    /* `size` bytes of the mods' block of EE memory (16-byte aligned, zeroed), 0 when it is full (56 KB in all, shared by
       the plugins on, kept for the whole run: take what you need once, in rac1_mod_main or the first level_start) */
    uint32_t (*ee_alloc)(uint32_t size);
    int (*replaying)(void);  /* 1 while the renderer replays a frame (see the rules above) */

    /* -- mobys (the game's objects: 0x100 bytes each; +0x10 the position x y z, +0x20 the state, +0x2c the scale, +0x40 the
          rotation, +0x78 the pvars (the class's own values), +0xa6 the class, +0xb2 the uid) -- */
    uint32_t (*moby_by_uid)(int32_t uid);  /* the moby with that uid on the level now, 0 when none (gone, or not spawned) */
    uint32_t (*moby_spawn)(int32_t cls);   /* a new moby of a class the level has (0: none); place it, then moby_moved */
    void (*moby_remove)(uint32_t moby);
    void (*moby_moved)(uint32_t moby);     /* its matrix from its position, rotation and scale (after writing them) */
    /* the moby a pvar word names (the level's links: a spawn index), 0 when none; and set one */
    uint32_t (*moby_link)(uint32_t moby, uint32_t pvar_offset);
    void (*moby_set_link)(uint32_t moby, uint32_t pvar_offset, uint32_t other);

    /* -- collision -- */
    /* the segment from -> to against the level's collision and the mobys: 1 and the point hit in `hit` (may be NULL),
       else 0 */
    int (*collide_line)(const float from[3], const float to[3], float hit[3]);

    /* -- the hero -- */
    uint32_t (*hero)(void);                          /* Ratchet's moby (0 when none) */
    void (*hero_place)(const float pos[3], float yaw);  /* put him there, as the game does at a level's start */
    int32_t (*bolts)(void);
    void (*bolts_add)(int32_t n);
    int32_t (*health)(void);

    /* -- sound and music -- */
    /* the level's sound `index` (its sound table: Veldin's), at `moby` (0: Ratchet); its slot or -1 */
    int32_t (*sound)(int32_t index, uint32_t moby);
    void (*music)(int32_t track);  /* the level's music table's track (as level.txt's music line set it) */

    /* -- effects -- */
    /* a puff of smoke from `pos` towards `vel` (Veldin's smoke: the base level's effects) */
    void (*smoke)(const float pos[3], const float vel[3], float size);

    /* -- the HUD (from the hud callback only) -- */
    /* `text` (plain ASCII) in the game's small font at (x, y) of the 512 x 448 HUD, colour 0xAABBGGRR (alpha 0x80 opaque) */
    void (*hud_text)(int32_t x, int32_t y, uint32_t abgr, const char* text);

    /* -- the camera -- */
    void (*camera_position)(float out[3]);

    /* -- the pad (pad 1) -- */
    uint32_t (*pad_held)(void);     /* the game's bits: 0x10 triangle, 0x20 circle, 0x40 cross, 0x80 square, 0x800 start,
                                       0x1000 up, 0x2000 right, 0x4000 down, 0x8000 left, 1 L2, 2 R2, 4 L1, 8 R1 */
    uint32_t (*pad_pressed)(void);  /* pressed this frame */

    /* -- random -- */
    /* a number 0..0xffffffff of the plugin's own (the game's random numbers untouched): the same each run, from the
       level's start */
    uint32_t (*random)(void);

    /* -- the game's code -- */
    /* calls the game's function at Veldin's address `veldin_addr` with one argument (a moby: the functions the game calls
       through a pointer, its objects' updates and the like), when the port has it: 1 called, 0 not (none such) */
    int (*call_port)(uint32_t veldin_addr, uint32_t arg);
} rac1_api;

#define RAC1_API_HAS(api, field) ((api)->size >= offsetof(rac1_api, field) + sizeof((api)->field))

/* Reads of the game's memory (inline): little-endian, as the PS2 */
static inline uint32_t rac1_lw(const rac1_api* a, uint32_t addr) {
    uint32_t v;
    memcpy(&v, a->ram + addr, 4);
    return v;
}
static inline uint16_t rac1_lh(const rac1_api* a, uint32_t addr) {
    uint16_t v;
    memcpy(&v, a->ram + addr, 2);
    return v;
}
static inline uint8_t rac1_lb(const rac1_api* a, uint32_t addr) { return a->ram[addr]; }
static inline float rac1_lwf(const rac1_api* a, uint32_t addr) {
    float v;
    memcpy(&v, a->ram + addr, 4);
    return v;
}

/* The plugin's callbacks (all optional: NULL is none). Append-only. */
typedef struct rac1_mod {
    uint32_t size;  /* set by the game to the size it knows; the plugin sets it to sizeof(rac1_mod) as it was built */
    void* user;     /* the plugin's: given back to every callback */

    void (*level_start)(void* user, const rac1_level_info* level);  /* a level's first frame (its objects spawned) */
    void (*level_end)(void* user);   /* the level is left (another one loads, a save is loaded) */
    void (*frame)(void* user);       /* once a frame of play (not in the menus), after the objects' updates */
    void (*hud)(void* user);         /* the HUD is drawn: api->hud_text here (may run twice a frame: draw only) */
    void (*camera)(void* user);      /* after the game's camera update: the camera's memory may be changed */
    /* a placed object of the plugin's own level (uid 1..objects) is about to be updated: return 1 to skip its class's own
       update (yours instead), 0 to let it run */
    int (*object_update)(void* user, uint32_t moby, int32_t uid, int32_t cls);
    void (*new_game)(void* user);    /* a new game starts (no save): reset your progress */
    /* a save is written: your bytes (at most `cap`) into `buf`, their count returned (0: none). They go into the game's
       save<N>.pc beside the memory card's save, under your mod's folder name */
    uint32_t (*save)(void* user, uint8_t* buf, uint32_t cap);
    /* a save is loaded: your bytes as saved (NULL / 0 when that save has none of yours: reset your progress) */
    void (*load)(void* user, const uint8_t* data, uint32_t size);
    /* testing: RAC1_MOD_TEST=<text> given, once a game frame */
    void (*test)(void* user, const char* text, uint64_t frame);
    void (*shutdown)(void* user);    /* the game ends */
} rac1_mod;

/* The ABI the plugin was built with (rac1_mod_abi): put RAC1_MOD_DECLARE_ABI() once in a source file */
#define RAC1_MOD_DECLARE_ABI() \
    RAC1_MOD_EXPORT unsigned rac1_mod_abi(void) { return RAC1_MOD_ABI; }

#ifdef __cplusplus
}
#endif

#endif /* RAC1_MOD_H */
