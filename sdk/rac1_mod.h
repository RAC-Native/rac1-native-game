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
 * The full reference (every struct, service and callback, the lifecycle, the limits): docs/sdk.md.
 *
 * Versioning: both tables only ever grow at their end (append-only). `api->size` is the game's table's size: before
 * calling a service added after version 1, check RAC1_API_HAS(api, field). `mod->size` is the plugin's: the game calls
 * only the callbacks that fit in it. Structs start with their size (set it to sizeof in the ones you fill).
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

/* The ABI this header describes. 1: the first; 2: the level's own object classes, ray and near queries, damage both
   ways, the mod's own sounds, triggers; 3: the base level's objects spawned at run time (base_spawn); 4: the vendor, the
   pad held from the game, the HUD's panels, anchors and text sizes; 5: Kalebo III's enemies (base_option, base_hunt for more
   classes), the weapons; 6: classes of other levels (a level mod's `import` lines: import_class, import_spawn, import_load,
   import_release); 7: reserved (the rails, still to come: its room is kept in the table below); 8: the level data of
   imported classes (path_find, path_create, object_bind_path, object_bind, group_create, object_slots); 9: sessions (a level
   mod of format 3: session_on, checkpoints, the respawn policy; the respawn, session_save and session_load callbacks); 10:
   sections (a session's sections and portals: section_load, section_ready, section_enter, section_unload, section_current,
   section_policy; the section_leave and section_enter callbacks); 11: the rails and the set-piece kit, in ABI 7's room of the
   table (rail_find .. fade_active; the rail callback); 12: hero contexts and modes (another planet's hero code, Ratchet as
   Clank, Giant Clank or on the hoverboard: hero_build, hero_build_now, hero_mode, hero_mode_now) (docs/sdk.md lists what each
   version added). */
#define RAC1_MOD_ABI 12

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

/* (ABI 4) hud_rect's / hud_text_ex's anchor: with a HUD wider than 4:3 (Options > Display > HUD) the element moves with the
   game's own side elements toward that edge of the screen; at 4:3 (the default) all three are the same */
#define RAC1_HUD_CENTRE 0   /* the 4:3 frame, in the middle of the screen (as hud_text) */
#define RAC1_HUD_LEFT 1     /* toward the screen's left edge */
#define RAC1_HUD_RIGHT 2    /* toward the screen's right edge */

/* (ABI 5) base_option's options */
#define RAC1_BASE_WEAPON 1  /* Kalebo III's Gadgetron robots (541): their weapon 0..2 */
#define RAC1_BASE_TROOPS 2  /* Kalebo III's dropships (1356): the jetpack troopers they bring, 0..4 */

/* (ABI 9) respawn_policy's policies: what a session's soft reset does with the level's placed objects (level.txt's moby
   lines; the base level's objects your plugin started are yours to handle in the respawn callback) */
#define RAC1_RESPAWN_RESET 0  /* those killed since the arrival come back as placed (not the ones the game keeps killed) */
#define RAC1_RESPAWN_KEEP 1   /* they stay as they are */

/* (ABI 10) section_policy's policies: what happens to a section's objects when Ratchet leaves it (level.txt's `section <name>
   remove|freeze|keep`, main's `leave`) */
#define RAC1_SECTION_REMOVE 0  /* taken out of play (the killed ones stay so when it is entered again, the others come back) */
#define RAC1_SECTION_FREEZE 1  /* kept, neither updated nor drawn */
#define RAC1_SECTION_KEEP 2    /* kept running */

/* (ABI 11) the rail callback's events */
#define RAC1_RAIL_ENTER 1  /* Ratchet is on the rail (he landed on it, jumped across to it, hero_rail_attach) */
#define RAC1_RAIL_LEAVE 2  /* he is off it (jumped off, fell, hopped off its end, another rail taken) */
#define RAC1_RAIL_END 3    /* he reached its end (its last point, riding along its points; its first riding back): before its LEAVE */

/* (ABI 11) Ratchet on a rail: hero_rail fills it */
typedef struct rac1_rail_info {
    uint32_t size;      /* the caller's sizeof (set it): the game fills what fits */
    int32_t rail;       /* the rail (rail_find's handle), 0 one of no level.txt line (a plugin's own table) */
    int32_t state;      /* his state: 0x28 grinding, 0x29 / 0x2a jumping, 0x2b attacking (the wrench), 0x42 hopping off its end */
    int32_t segment;    /* the rail's point he is past (0..) */
    float distance;     /* how far along the rail from its first point, in units */
    float length;       /* the rail's length */
    float speed;        /* units a frame (at 60 frames a second) */
    int32_t direction;  /* 1: riding along the points' order, -1 back */
    float pos[3];       /* the point of the rail under him */
} rac1_rail_info;

/* (ABI 11) camera_set's modes */
#define RAC1_CAMERA_FIXED 0  /* at `pos`, turned `yaw` (radians about z, 0 looking along +x) and `pitch` (radians, up positive) */
#define RAC1_CAMERA_LOOK 1   /* at `pos`, looking at `target`, or at the moby `moby` when not 0 (it follows it) */
#define RAC1_CAMERA_PATH 2   /* along the path `path` (path_find / path_create) at `speed` units a second (`loop` 1: round again,
                                else it stops at its end), looking at `target` / `moby`, or ahead along the path when both are
                                none (target all 0 and moby 0) */
#define RAC1_CAMERA_FOLLOW 3 /* behind the moby `moby` (Ratchet's when 0): `pos` its offset in the moby's frame (x ahead, y left, z
                                up), looking at the moby `target[2]` units over its feet */

/* (ABI 11) A camera of the plugin's: camera_set takes it */
typedef struct rac1_camera {
    uint32_t size;      /* the caller's sizeof (set it) */
    int32_t mode;       /* RAC1_CAMERA_* */
    float pos[3];
    float target[3];
    uint32_t moby;
    float yaw, pitch;   /* (FIXED) */
    int32_t path;       /* (PATH) */
    float speed;        /* (PATH) units a second */
    int32_t loop;       /* (PATH) */
    int32_t blend;      /* frames from the camera before to this one (0: a cut) */
} rac1_camera;

/* (ABI 11) hazard_add's shapes */
#define RAC1_HAZARD_BOX 0     /* lo .. hi */
#define RAC1_HAZARD_SPHERE 1  /* the centre lo, the radius hi[0] */

/* (ABI 11) A hazard: a volume that hurts Ratchet while he is in it, on a timer */
typedef struct rac1_hazard {
    uint32_t size;      /* the caller's sizeof (set it) */
    int32_t shape;      /* RAC1_HAZARD_* */
    float lo[3], hi[3];
    int32_t damage;     /* health points a hurt (the game's grace time after a hurt keeps it from hurting every frame) */
    int32_t on_frames;  /* on for so many frames, then off for off_frames, round again (0: always on) */
    int32_t off_frames;
    int32_t phase;      /* frames into its round at its start */
    uint32_t object;    /* the level object it moves with (its volume relative to the object's place), 0 none */
} rac1_hazard;

/* (ABI 11) object_follow_path's modes */
#define RAC1_PATH_ONCE 0      /* to its last point, then it stops */
#define RAC1_PATH_LOOP 1      /* from its last point to its first, round again */
#define RAC1_PATH_PINGPONG 2  /* back and forth */

/* (ABI 12) hero_mode's modes: Ratchet as himself, as Clank, as Giant Clank, on a hoverboard */
#define RAC1_HERO_NONE 0
#define RAC1_HERO_CLANK 1       /* riding an object of Orxon's Clank class (import "10:87") */
#define RAC1_HERO_GIANT_CLANK 2 /* riding an object of Quartu's Giant Clank class (import "15:419") */
#define RAC1_HERO_BOARD 4       /* on a hoverboard (import "16:439"), with Kalebo III's hero code (hero_build(16)) */
/* (ABI 12) hero_build's generic hero code (Veldin's, Gaspar's, Umbris's: their lava), on a level mod of any format whose base
   level has hero code of its own (Kalebo III, Quartu, Rilgar) */
#define RAC1_HERO_BUILD_GENERIC (-2)

/* (ABI 9) A session's respawn: given to the respawn callback */
typedef struct rac1_respawn {
    uint32_t size;           /* sizeof the game's struct */
    uint32_t count;          /* the session's respawns so far, this one included */
    const char* checkpoint;  /* the checkpoint Ratchet is back at (level.txt's name, checkpoint_at's, "game", "start") */
    float pos[3];            /* where */
    float yaw;
} rac1_respawn;

/* (ABI 2) A level object's spawn: given to its class's spawn callback */
typedef struct rac1_spawn {
    uint32_t size;         /* sizeof the game's struct */
    float pos[3];          /* where (the game's coordinates) */
    float yaw;             /* turned (radians, about z) */
    const char* params;    /* a placed object's key=value words as level.txt has them ("" when none: object_spawn's) */
    int32_t placed;        /* 1: placed by level.txt's object line, 0: object_spawn */
    int32_t index;         /* (placed) its object line's index, 0.. in the lines' order; -1 */
} rac1_spawn;

/* (ABI 2) A hit taken (the game's hit record: the player's attacks, an enemy's, hit_send's) */
typedef struct rac1_hit {
    uint32_t size;         /* the hit callback: the game's sizeof; hit_take: the caller's (set it), filled as it fits */
    uint32_t from;         /* the attacker's moby (0: none known) */
    uint32_t kinds;        /* the record's kind bits (RAC1_HIT_*) */
    float strength;        /* the record's strength: the game's damage value */
    float point[3];        /* where (when the attack says; else 0 0 0) */
    float dir[3];          /* its push (may be 0 0 0) */
} rac1_hit;

/* hit kinds (the game's hit records' bits): the player's attacks reaching an object, Ratchet's own hurt */
#define RAC1_HIT_HERO 0x00000001u     /* (a hit on Ratchet: hero_damage's) */
#define RAC1_HIT_PLAYER 0x01b30000u   /* any of the player's attacks (weapons, the wrench, explosions) */
#define RAC1_HIT_ENEMY 0x00330000u    /* what the game's enemies take */
#define RAC1_HIT_PROP 0x00010000u     /* what the game's breakable props take */

/* (ABI 2) What a ray hit (collide_ray) */
typedef struct rac1_ray_hit {
    uint32_t size;         /* the caller's sizeof (set it; 0: the game's): the game fills what fits */
    float point[3];        /* the point hit */
    float normal[3];       /* the surface's normal there (unit) */
    uint32_t moby;         /* the moby hit, 0: the level's collision */
    uint32_t object;       /* the level object whose moby that is, 0: none */
} rac1_ray_hit;

/* collide_ray's flags (the game's segment test's) */
#define RAC1_RAY_NO_LEVEL 0x01u       /* the mobys only (not the level's collision) */
#define RAC1_RAY_HITTABLE 0x08u       /* only the mobys that take hits */
#define RAC1_RAY_BACKFACES 0x10u      /* the level's triangles from behind too */

/* (ABI 2) A level object class's behaviour: class_define (in rac1_mod_main), bound by name to level.txt's class line */
typedef struct rac1_class_def {
    uint32_t size;         /* the plugin's sizeof(rac1_class_def) */
    const char* name;      /* level.txt's class name (copied) */
    uint32_t state_size;   /* bytes of state per object (api->object_state), zeroed at its spawn; 0: none */
    void* user;            /* given back to the callbacks */
    /* an object of the class appears: placed ones at the level's start (before level_start), spawned ones in
       object_spawn */
    void (*spawn)(void* user, uint32_t obj, const rac1_spawn* spawn);
    /* once a frame of play, in the objects' pass (where a moby's class runs), after hit */
    void (*update)(void* user, uint32_t obj);
    /* the object took a hit this frame (the player's attacks: RAC1_HIT_PLAYER, or hit_send's), before its update */
    void (*hit)(void* user, uint32_t obj, const rac1_hit* hit);
    /* the object goes (object_remove, or the level is left): its state is freed after */
    void (*remove)(void* user, uint32_t obj);
} rac1_class_def;

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
    /* `size` bytes of the mods' block of EE memory (16-byte aligned, zeroed), 0 when it is full (53 KB in all, shared by
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
    void (*hero_place)(const float pos[3], float yaw);  /* put him there standing, as the game's teleports do */
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

    /* ======== ABI 2 (test RAC1_API_HAS(api, class_define) to run on an ABI 1 game too) ======== */

    /* -- the level's object classes and objects (docs/sdk.md, "Objects"). An object is a handle, 0 none; a removed
          object's handle never names another. Objects exist on a level mod's level only (its level.txt's class lines) -- */
    /* the behaviour of level.txt's class `def->name`: call in rac1_mod_main; 1 done, 0 not (no name, too many) */
    int (*class_define)(const rac1_class_def* def);
    int32_t (*class_find)(const char* name);  /* the current level's class of that name: its index, -1 none */
    /* a new object of class `cls` at `pos` turned `yaw`: its handle (its class's spawn called first), 0 when none can be
       made (no such class, the game's mobys all used) */
    uint32_t (*object_spawn)(int32_t cls, const float pos[3], float yaw);
    void (*object_remove)(uint32_t obj);       /* (its class's remove called) */
    uint32_t (*object_moby)(uint32_t obj);     /* its moby (position +0x10, rotation +0x40, ...: the moby services work) */
    uint32_t (*object_of_moby)(uint32_t moby); /* the object a moby carries, 0 none */
    int32_t (*object_class)(uint32_t obj);     /* its class's index, -1: no such object */
    void* (*object_state)(uint32_t obj);       /* its state (its class's state_size bytes), NULL none */
    /* a placed object's key=value word `key`: its value into `out` (`cap` bytes); 1 found, 0 not */
    int (*object_param)(uint32_t obj, const char* key, char* out, size_t cap);
    uint32_t (*object_named)(const char* name);  /* the placed object whose name=<name>, 0 none */
    /* moved: its position (NULL: kept), its rotation (x y z, radians; NULL: kept), its scale (<= 0: kept) */
    void (*object_place)(uint32_t obj, const float pos[3], const float rot[3], float scale);
    void (*object_show)(uint32_t obj, int on);   /* drawn or not (its collision stays) */
    void (*object_solid)(uint32_t obj, int on);  /* collision on (stood on, bumped, hit) or off */
    int32_t (*object_count)(void);               /* the level's objects now */
    uint32_t (*object_at)(int32_t i);            /* the i-th (0..object_count - 1), 0 past them */

    /* -- queries -- */
    /* the segment from -> to against the level's collision and the mobys (`ignore`: a moby skipped, 0 none; `flags`:
       RAC1_RAY_*): 1 and `out` filled (may be NULL) when something is hit, else 0 */
    int (*collide_ray)(const float from[3], const float to[3], uint32_t ignore, uint32_t flags, rac1_ray_hit* out);
    /* the level objects within `radius` of `pos` (their positions): at most `max` into `out`, nearest first; the count */
    int32_t (*objects_near)(const float pos[3], float radius, uint32_t* out, int32_t max);
    /* the live mobys within `radius` of `pos` (class `cls`, -1 any; Ratchet included): as objects_near */
    int32_t (*mobys_near)(const float pos[3], float radius, int32_t cls, uint32_t* out, int32_t max);

    /* -- damage -- */
    /* Ratchet hurt by `amount` (health points) from moby `from` (0: none; its position gives the push): the game's own
       hurt (his state, the knock back, a hit's grace time: 0 when he cannot be hurt now) */
    int (*hero_damage)(int32_t amount, uint32_t from);
    /* a hit record on `moby` (as the game's attacks write them): `kinds` (RAC1_HIT_*), `strength`, the attacker */
    void (*hit_send)(uint32_t moby, uint32_t kinds, float strength, uint32_t from);
    /* the hit `moby` took (one of `kinds`), into `out` (may be NULL), and consumed: 1, else 0 */
    int (*hit_take)(uint32_t moby, uint32_t kinds, rac1_hit* out);

    /* -- the mod's own sounds (level.txt's sound lines; docs/sdk.md, "Sounds") -- */
    int32_t (*sfx_find)(const char* name);  /* the current level's sound of that name: its index, -1 none */
    /* sound `sound` on `moby` (it follows it), else at `pos`, else (both 0 / NULL) everywhere: the game's sound slot
       (distance, pan, occlusion as its own sounds), -1 when none (too far to be heard, all slots busy) */
    int32_t (*sfx_play)(int32_t sound, uint32_t moby, const float pos[3]);
    void (*sfx_stop)(int32_t slot);  /* a slot sfx_play gave (a looping sound plays until stopped) */
    /* 1 while slot `slot` still plays the level's sound `sound` (a sound ends, a looping one far from the camera is let
       go by the game: play it again when it is near) */
    int (*sfx_active)(int32_t slot, int32_t sound);

    /* -- triggers (level.txt's trigger lines) -- */
    int32_t (*trigger_find)(const char* name);  /* its index, -1 none */
    int (*trigger_inside)(int32_t index);       /* 1 while Ratchet is inside */

    /* -- time -- */
    uint64_t (*frame_count)(void);  /* the frames of play since the game started (the frame callback's count) */

    /* ======== ABI 3 (test RAC1_API_HAS(api, base_spawn)) ======== */

    /* -- the base level's objects at run time (docs/sdk.md, "Base level objects"): a level mod's level.txt `spawn <class>
          <count>` lines give room for <count> live objects of the base level's class <class> at once -- */
    /* a new object of the base level's class `cls` at `pos` turned `yaw`, as the base level's first placement of the class
       (its settings, its pvars, its bolts), its class's own behaviour: a handle, 0 when none (no spawn line for the class,
       all of its room in use) */
    uint32_t (*base_spawn)(int32_t cls, const float pos[3], float yaw);
    /* its moby while it lives, 0 once it is gone (killed, removed, the level left): its death */
    uint32_t (*base_moby)(uint32_t handle);
    /* how many more of class `cls` base_spawn can start now (0: none, or no spawn line for it) */
    int32_t (*base_free)(int32_t cls);

    /* ======== ABI 4 (test RAC1_API_HAS(api, vendor_open)) ======== */

    /* -- the vendor -- */
    /* the vendor's screens (buying weapons and ammo), as the game's own vendor item opens them (no Vendor moby needed): 1
       opened, 0 not (not in play: a menu, a cutscene, the vendor already open; or called from the HUD callback or while
       replaying). Call it from the frame callback (or an object's update or a trigger callback). The game is paused under
       them: the frame and hud callbacks are not called until they close */
    int (*vendor_open)(void);

    /* -- the pad held -- */
    /* on: the game gets no input from the pad (Ratchet neither moves nor jumps nor attacks, the camera stick does nothing)
       but Start and Select (the pause menu, the map: quitting works as always), from the next frame on, in play only (the
       menus, the vendor's screens and the cutscenes get the pad as always); pad_held / pad_pressed read the real pad. Off:
       the game gets the pad again, but the buttons still held at that moment, until they are released (a Cross that closed
       your menu does not make him jump). Off again at every level's start */
    void (*input_hold)(int on);

    /* -- the HUD (from the hud callback only) -- */
    /* a flat rectangle w x h from (x, y) (512 x 448 HUD units, as hud_text), colour 0xAABBGGRR, alpha 0x80 opaque, 0x40 half
       transparent: a panel behind your texts (draw it first). anchor: RAC1_HUD_* */
    void (*hud_rect)(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t abgr, int32_t anchor);
    /* hud_text at `scale` times the font's size (1: hud_text's, 2: twice as large; at most 8), anchor RAC1_HUD_* */
    void (*hud_text_ex)(int32_t x, int32_t y, uint32_t abgr, float scale, int32_t anchor, const char* text);
    /* the width of `text` in HUD units at `scale` (to centre or right-align it) */
    int32_t (*hud_text_width)(const char* text, float scale);

    /* -- the base level's enemies -- */
    /* on: the moby `moby` (a base level's enemy: base_moby's, a moby line's) hunts Ratchet wherever he is, instead of
       guarding the place where it started (docs/sdk.md, "Base level objects"); off: as its class does. 1 done, 0 not (no
       such moby, or a class the game does not know how to: Kalebo III's robots 1445; from ABI 5 also base_spawn's bomb bots
       52, Gadgetron robots 541, jetpack troopers 638 and dropships 1356). Kept until the moby is gone */
    int (*base_hunt)(uint32_t moby, int on);

    /* -- lights -- */
    /* a light for this frame (call it every frame it shines, from the frame callback): at `pos`, colour `rgb` (0..1 each)
       times `intensity` (0..8; 1 as a lamp placed in the debug menu's Lights tab), reaching `radius` units (at most 60).
       Drawn by the modern lighting (Options > Graphics: Lighting modern, Secondary Lights on) per pixel, as the lights placed by hand, without
       shadows; the faithful renderer and point lights off draw none. 31 at most a frame for all the plugins; 1 taken,
       0 not (full, or bad values) */
    int (*light_add)(const float pos[3], const float rgb[3], float intensity, float radius);

    /* ======== ABI 5 (test RAC1_API_HAS(api, base_option)) ======== */

    /* -- the base level's enemies -- */
    /* a choice of the class's for an object base_spawn has just started (call it in the same callback, before its first
       update): RAC1_BASE_WEAPON (Kalebo III's Gadgetron robots 541: 0 the launcher, 1 the club, 2 the flamethrower), 1 done;
       RAC1_BASE_TROOPS (Kalebo III's dropships 1356: `value` jetpack troopers 638, at most 4, started from 638's spawn room
       and carried until it drops them): how many it took. 0: not (no such object, already updated, a class or value the
       option has not) */
    int (*base_option)(uint32_t handle, int32_t option, int32_t value);
    /* the handle of a moby base_spawn started (one a dropship brought: find them with mobys_near), 0 none */
    uint32_t (*base_handle)(uint32_t moby);

    /* -- the weapons (the item ids of the pause menu's Weapons page: 0x09 Suck Cannon, 0x0a Bomb Glove, 0x0b Devastator,
          0x0d Visibomb Gun, 0x0e Taunter, 0x0f Blaster, 0x10 Pyrocitor, 0x11 Mine Glove, 0x12 Walloper, 0x13 Tesla Claw,
          0x14 Glove of Doom, 0x15 Morph-o-Ray, 0x17 R.Y.N.O., 0x18 Drone Device, 0x19 Decoy Glove) -- */
    /* the weapons Ratchet owns: at most `max` ids into `out` (may be NULL), in id order; how many he owns */
    int32_t (*weapons_owned)(int32_t* out, int32_t max);
    /* that weapon in his hand, as the quick select puts it there (the game's switch: its animation and sound, the next
       frames): 1 asked (or in hand already), 0 not (not owned, not allowed by weapon_allow, not in play, or where the quick
       select could not be opened now: swinging or throwing the wrench, switching or using an item, in a vehicle, a ship, on
       a hoverboard; call it again later) */
    int (*weapon_equip)(int32_t item);
    /* only the `n` weapons of `items` can be chosen: the others are not in the quick select's ring, the PC's next / previous
       weapon skip them, and one asked otherwise (the game's own return to a weapon) is not taken; one in hand goes for the
       last allowed one held (else the wrench), unless a switch to an allowed one is under way (weapon_equip's, before or
       after this call): that one ends in his hand. NULL or 0: all again. Off at every level's start */
    void (*weapon_allow)(const int32_t* items, int32_t n);
    /* that item's ammo to its most (the HUD's, the pickups'): 1, 0 not (not owned, an item with no ammo) */
    int (*ammo_fill)(int32_t item);

    /* ======== ABI 6 (test RAC1_API_HAS(api, import_class)) ======== */
    /* -- classes of other levels (docs/open-engine.md): a level mod's level.txt `import <name>` / `import <level>:<class>`
          lines bring them, with what they spawn (their projectiles, effects, drops), into the level at its load. `name` is
          one of the game's data/pool/names.txt ("gemlik.turret") or "<level>:<class>" -- */
    /* the class id the import `name` has in the level now (its own id, the same as on its level, unless the base level has
       that id: then one of 2048 and up); the base level's own class when `name` is one of its; -1 when it is not in the level */
    int32_t (*import_class)(const char* name);
    /* an object of that class (import_class's id) started at `pos` turned `yaw` radians, as its level first places one (its
       size, its settings, its private values: what names other objects of its level, or its level's paths, is none); it
       drops its bolts when killed, and is gone for good once killed. Its moby's address (as mobys_near's), 0 when it cannot
       be (no such class, no room) */
    uint32_t (*import_spawn)(int32_t cls, const float pos[3], float yaw);
    /* the class `name` brought into the level now, with what it spawns (it is drawn from the next frame): its id, -1 when it
       cannot be (no such class, the class table full, no pool) */
    int32_t (*import_load)(const char* name);
    /* the class `name` taken out of the level: its objects removed at once, its memory given back a few frames later, the
       classes only it needed with it; how many classes went (0: not imported) */
    int32_t (*import_release)(const char* name);

    /* ======== ABI 7's room: the rails and the set-piece kit (game ABI 11). On a game of ABI 8 to 10 these 16 entries are there
       but NULL: test api->abi >= 11 (RAC1_API_HAS(api, rail_find) is true on those games too) ======== */
    /* -- grind rails (docs/sdk.md, "Rails"): level.txt's `rail` / `rails` lines; a rail is in the game's rail table while its
          section is the current one (main's always in a level without sections) -- */
    /* the rail `name` (level.txt's name, a rails file's object's): its handle, 0 none */
    int32_t (*rail_find)(const char* name);
    /* 1 while Ratchet grinds (on a rail, jumping along it, hopping off its end), `out` filled (may be NULL); 0 not */
    int (*hero_rail)(rac1_rail_info* out);
    /* the point `distance` units along the rail from its first point (clamped to it; a loop's wraps) into `out`, its direction
       there (unit) into `dir` (either may be NULL): 1, 0 no such rail */
    int (*rail_point)(int32_t rail, float distance, float out[3], float dir[3]);
    /* the rail taken out of the game's search (off: Ratchet cannot land on it or jump to it: a broken rail, a gate) or back: 1 */
    int (*rail_enable)(int32_t rail, int on);
    /* Ratchet put on the rail `distance` units along it, riding `direction` (1 along its points, -1 back), as if he had landed
       on it: 1 asked (he is on it the next frame), 0 not (no such rail, not in the current section, not in play, no Grind
       Boots: give them with item_visit) */
    int (*hero_rail_attach)(int32_t rail, float distance, int32_t direction);
    /* Ratchet's speed on the rail times `scale` (0.25 .. 4; 1 the game's): 1 */
    int (*rail_speed)(int32_t rail, float scale);

    /* -- items for the visit (level.txt's `items` line does the same at the level's start) -- */
    /* item `item` (the game's item ids: 0x1c Magneboots, 0x1d Grind Boots, ...) Ratchet's while he is on this level (on), or
       taken back (off: only one this call or level.txt gave): never in a save (the card's save is written without it), taken
       back when he leaves. 1 done, 0 not (owned already: his own; not a level mod's level) */
    int (*item_visit)(int32_t item, int on);

    /* -- the screen and the camera -- */
    /* the screen faded to the colour `rgb` (0xBBGGRR) over `out_frames`, held `hold_frames`, faded back over `in_frames`
       (the HUD under it): 1 */
    int (*fade)(int32_t out_frames, int32_t hold_frames, int32_t in_frames, uint32_t rgb);
    /* the camera the plugin's (`cam`: RAC1_CAMERA_*), from the next frame, until camera_release (or the level's end): 1, 0 bad */
    int (*camera_set)(const rac1_camera* cam);
    /* the game's camera back, blended over `blend_frames` (0: a cut): 1 */
    int (*camera_release)(int32_t blend_frames);

    /* -- hazards: volumes that hurt Ratchet -- */
    /* a hazard (its handle, 0 none: bad, more than 64) */
    int32_t (*hazard_add)(const rac1_hazard* h);
    /* the hazard on (1) or off (0), or removed (-1): 1, 0 no such */
    int (*hazard_set)(int32_t hazard, int on);

    /* -- moving platforms: a level object moved along a path every frame (in the objects' pass, so Ratchet standing on it rides
          it) -- */
    /* the level object `obj` (a class with collision: its box or its mesh) moved along the path `path` (path_find /
       path_create) at `speed` units a second from its first point, RAC1_PATH_*; path 0 or speed 0: stopped where it is. 1 */
    int (*object_follow_path)(uint32_t obj, int32_t path, float speed, int32_t mode);
    /* how far along its path the object is (units), -1 none (not following one); at its end (RAC1_PATH_ONCE) its length */
    float (*object_path_at)(uint32_t obj);

    /* -- more -- */
    /* the rail `rail`'s name into `name` (`cap` bytes, may be NULL), its length (may be NULL), its section's index (0 main;
       may be NULL): 1, 0 no such */
    int (*rail_info)(int32_t rail, char* name, int32_t cap, float* length, int32_t* section);
    /* the fade now: 0 none, 1 fading out, 2 held (the screen covered), 3 fading back */
    int (*fade_active)(void);

    /* ======== ABI 8 (test RAC1_API_HAS(api, object_bind_path)) ======== */
    /* -- the level data of imported classes (docs/open-engine.md, "Class profiles"): an imported object's private values that
          name its level's paths, areas, groups or other objects are its class's slots, named by its profile
          (data/pool/profiles in the game's folder: "patrol", "area", "door"...); import_spawn gives them the profile's
          defaults (a square round the object, points ahead of it, the whole level, none); these change them. A class
          without a profile has no slots -- */
    /* level.txt's `path <name> x y z x y z ...` line `name`: its handle, 0 when the level has none */
    int32_t (*path_find)(const char* name);
    /* a path of the plugin's own: `n` points (1 to 64), xyz[3 * n] in the game's coordinates: its handle, 0 when it cannot be */
    int32_t (*path_create)(const float* xyz, int32_t n);
    /* the slot `slot` (a path or an area) of the imported object `moby` (import_spawn's) given the path `path` (path_find's,
       path_create's): its points copied into the object's level's data (its own path there). 1 done, 0 not (no such slot or
       object, not a path's slot, no room) */
    int (*object_bind_path)(uint32_t moby, const char* slot, int32_t path);
    /* the slot `slot` of the imported object `moby` set to `value`: a moby's slot takes another object's moby address (0:
       none), a group's slot a group_create handle, a word's slot the word. 1 done, 0 not */
    int (*object_bind)(uint32_t moby, const char* slot, uint32_t value);
    /* a group of `n` objects (moby addresses) for the imported object `owner`'s class (in its level's data, where its code
       reads groups): the group's handle for object_bind (-1 when it cannot be) */
    int32_t (*group_create)(uint32_t owner, const uint32_t* mobys, int32_t n);
    /* the slots of the imported object `moby`'s class, "name:kind name:kind ..." (kind: path, area, group, moby, word, vec3)
       into `out` (`cap` bytes): their count, 0 when it has no profile */
    int32_t (*object_slots)(uint32_t moby, char* out, int32_t cap);

    /* ======== ABI 9 (test RAC1_API_HAS(api, session_on)) ======== */
    /* -- sessions (docs/open-engine.md, "Sessions"): a level mod of format 3 is loaded once, at the arrival; a death is a soft
          reset (Ratchet back at the current checkpoint, no level load: his weapons, ammo and bolts, your state and your
          objects kept; the respawn callback tells you) -- */
    /* 1: the level is a session (format 3), 0 not (the services below then do nothing) */
    int (*session_on)(void);
    /* level.txt's `checkpoint <name> x y z [yaw]` the current one (where the next respawn puts Ratchet): 1, 0 none of that
       name */
    int (*checkpoint_set)(const char* name);
    /* a checkpoint of your own at `pos` turned `yaw` (radians), named `name` (NULL: "plugin") the current one: 1 done */
    int (*checkpoint_at)(const float pos[3], float yaw, const char* name);
    /* the current checkpoint: its place and yaw (either may be NULL), its name into `name` (`cap` bytes, may be NULL); the
       session's respawns so far, -1 outside a session */
    int32_t (*checkpoint_get)(float pos[3], float* yaw, char* name, int32_t cap);
    /* the level's placed objects at a respawn: RAC1_RESPAWN_RESET (level.txt's `respawn reset`, the default) or
       RAC1_RESPAWN_KEEP; -1 only asks. The policy now, -1 outside a session */
    int32_t (*respawn_policy)(int32_t policy);

    /* ======== ABI 10 (test RAC1_API_HAS(api, section_enter)) ======== */
    /* -- sections (docs/open-engine.md, "Sections"): a session's level.txt `section <name> ... end` blocks, each its own
          collision, meshes, sky, lights, music and objects; "main" is the lines outside them (where the ship lands). A switch
          keeps everything of the session (Ratchet, his weapons, your state); only the current section is drawn -- */
    /* section `name` streamed in (read on a worker, committed a little each frame): 1 started or in already, 0 no such section
       (every section is streamed in at the arrival: call it after section_unload) */
    int (*section_load)(const char* name);
    /* 1: section `name` is in (a switch to it is made at once), 0 not yet, -1 no such section */
    int (*section_ready)(const char* name);
    /* Ratchet to section `name`'s entry `entry` (NULL or "": its first), the screen faded out and in over `fade` frames (0:
       none), at the next frame's start once the section is in: 1 asked, 0 no such section or entry. To an entry of the
       current section: Ratchet put there */
    int (*section_enter)(const char* name, const char* entry, int32_t fade);
    /* section `name` unloaded (its objects out of play, its memory given back): 1 done, 0 not (main, the current one, no such) */
    int (*section_unload)(const char* name);
    /* the current section: its name into `name` (`cap` bytes, may be NULL); its index (0 main), -1 no session with sections */
    int32_t (*section_current)(char* name, int32_t cap);
    /* section `name`'s objects when it is left: RAC1_SECTION_REMOVE, _FREEZE or _KEEP; -1 only asks. The policy now, -1 no such */
    int32_t (*section_policy)(const char* name, int32_t policy);

    /* ======== ABI 12 (test RAC1_API_HAS(api, hero_build)) ======== */
    /* -- hero contexts and modes (docs/sdk.md, "Hero modes"; a session only): another planet's own hero code, Ratchet as Clank,
          Giant Clank or on a hoverboard. level.txt's `hero` line does the same for a section -- */
    /* (a session) the hero code and camera code of level `level` run Ratchet from now on, as on that planet: 16 Kalebo III (its
       hero rides the hoverboard), 15 Quartu, 13 Gemlik, 10 Orxon, 5 Rilgar; -1 the session's own (Veldin's);
       RAC1_HERO_BUILD_GENERIC the generic code on a level mod of any format on such a base. 1 done, 0 not (no session, a level
       without hero code of its own). Off a hoverboard first when the new code has none */
    int (*hero_build)(int32_t level);
    /* the level whose hero code runs Ratchet now, -1 the session's own */
    int32_t (*hero_build_now)(void);
    /* Ratchet in mode `mode` riding the object `obj` (RAC1_HERO_CLANK, _GIANT_CLANK: it becomes the hero, his health and weapons
       kept for his return; RAC1_HERO_BOARD: on it, with hero_build(16)); RAC1_HERO_NONE (obj ignored): out of the mode he is
       in, back as Ratchet where the object is. 1 done, 0 not (dying, already in a mode, no such object) */
    int (*hero_mode)(int32_t mode, uint32_t obj);
    /* Ratchet's mode now (RAC1_HERO_*) */
    int32_t (*hero_mode_now)(void);
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

    /* ======== ABI 2 ======== */
    /* Ratchet entered (inside 1) or left (0) level.txt's trigger `index` (its `name`), before the frame callback */
    void (*trigger)(void* user, int32_t index, const char* name, int32_t inside);

    /* ======== ABI 9 ======== */
    /* (a session) Ratchet died and is back at the checkpoint: a soft reset, no level load (level_start and level_end are not
       called; his weapons, ammo and bolts, your state and the objects you started are as they were): bring back what the
       battle he died in needs (its enemies), before the next frame */
    void (*respawn)(void* user, const rac1_respawn* r);
    /* (a session) a save is written in it: the session's state of yours (where the player is in it: a wave, a door opened),
       at most `cap` bytes into `buf`, their count returned (0: none); kept with the session's item in save<N>.pc, apart
       from your save callback's progress */
    uint32_t (*session_save)(void* user, uint8_t* buf, uint32_t cap);
    /* (a session) a save made in it was loaded: your session bytes as saved, after the level's level_start */
    void (*session_load)(void* user, const uint8_t* data, uint32_t size);

    /* ======== ABI 10 ======== */
    /* (sections) Ratchet left section `name` (a portal, section_enter, a death in another section's checkpoint), before the
       next section's section_enter */
    void (*section_leave)(void* user, const char* name);
    /* (sections) Ratchet is in section `name` now, at its entry `entry` ("" when none: a death's checkpoint, a save's) */
    void (*section_enter)(void* user, const char* name, const char* entry);

    /* ======== ABI 11 ======== */
    /* Ratchet and a rail of level.txt's (`rail`: rail_find's handle, `name` its name): RAC1_RAIL_ENTER, _END, _LEAVE, after the
       hero's update, before the frame callback */
    void (*rail)(void* user, int32_t rail, const char* name, int32_t event);
} rac1_mod;

/* The ABI the plugin was built with (rac1_mod_abi): put RAC1_MOD_DECLARE_ABI() once in a source file */
#define RAC1_MOD_DECLARE_ABI() \
    RAC1_MOD_EXPORT unsigned rac1_mod_abi(void) { return RAC1_MOD_ABI; }

#ifdef __cplusplus
}
#endif

#endif /* RAC1_MOD_H */
