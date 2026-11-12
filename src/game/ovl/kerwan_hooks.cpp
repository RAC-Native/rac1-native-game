// See kerwan_hooks.h.
#include <game/ovl/kerwan_hooks.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// orig: (host) see kerwan_hooks.h
bool kerwan_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L3_CASE(0x002ed800): emitter_reverb_box_update(em); return true;
    case L3_CASE(0x002ed960): emitter_music_box_update(em); return true;
    default: return false;
    }
}

}  // namespace rac1
