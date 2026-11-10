// The open engine's contexts (docs/open-engine.md, stage 2): level A's own code running in a session whose loaded overlay
// is another level's (the session's base: Veldin's). Every level-only address of A's code is a site that names its level
// (`L<A>_ADDR(a)`, ovl_reloc.h's region_site); with a context for A those sites translate through it:
//   - an address of the base executable (below the overlays, 0x15ef00): itself;
//   - A's engine data (inside an engine object of A's map: the extents, overlay_map/levelA.ext): the session's object
//     (A -> Veldin through the extents; the session's map is Veldin's own, the identity, until a session enlarges a table);
//   - A's own data: the same place in A's context image, a copy of A's data sections 0..5 in the session heap
//     (the pool's context/levelNN.img) whose pointer words are relocated by the same rules;
//   - A's code: a function A's map pairs with Veldin's (or a variant standing in for one), Veldin's function (the port
//     runs it); A's own code, a tag (ovl_reloc.h: ctx_tag), which the dispatchers resolve to A's ports.
// Nothing here runs unless a session creates a context: on the original's levels no context exists and every site is
// what it always was.
#ifndef RAC1_GAME_ENHANCE_CONTEXT_H
#define RAC1_GAME_ENHANCE_CONTEXT_H

#include <cstdint>

namespace rac1 {

// What an address of level A is, in A's context
enum class CtxKind {
    Base,        // the base executable's (below 0x15ef00): the same on every level
    EngineData,  // inside one of A's engine objects: Veldin's object
    OwnData,     // A's own data (its data sections, in no engine object): A's context image
    EngineCode,  // A's code that A's map pairs with Veldin's: Veldin's function
    OwnCode,     // A's own code: a tag
    Outside,     // past A's code (A's layout: the level's run-time allocations): kept, logged
};
// orig: (host) the kind's name for the logs and the sites report
const char* ctx_kind_name(CtxKind k);

// orig: (host) level `level`'s context made (once; the session's memory must be on, the pool's context image there): its
// image copied into the session heap and relocated, its sites translated through it from now on. False when it cannot be
// made (logged); true when it exists already. The session's base level (the loaded overlay's) needs none
bool ctx_create(int level);
// orig: (host) every context dropped (a level's full setup: a new session base): their heap blocks freed, the sites native
void ctx_drop_all();
// orig: (host) level `level` has a context
bool ctx_exists(int level);
// orig: (host) where level `level`'s context image is (its address CONTEXT_BASE there), 0 without a context
uint32_t ctx_image(int level);
// orig: (host) level `level`'s address `a` classified in its context, with its translation in `*to` (a session address,
// a Veldin function, a tag); the level must have a context
CtxKind ctx_classify(int level, uint32_t a, uint32_t* to);
// orig: (host) the translation of level `level`'s address `a` through its context (a site's: ovl_reloc.h's
// g_ctx_xlate); `a` when there is no context
uint32_t ctx_xlate(int level, uint32_t a);
// orig: (host) a view of level `level`'s own (DESIGN 6.2's registries, their first form): the engine object at Veldin's
// `veldin` (`size` bytes) is, for level `level`'s sites, the copy at `to` (a level's paths seen by its own classes, the
// session's engine keeping its own); `to` 0 drops the view. The level must have a context
bool ctx_view(int level, uint32_t veldin, uint32_t size, uint32_t to);

}  // namespace rac1

#endif
