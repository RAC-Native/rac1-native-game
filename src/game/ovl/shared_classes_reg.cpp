// The registration of the shared classes' updates (veldin_shared_classes.cpp, formerly the generated shared_classes_tr.cpp): kept out of
// that file (written when it was generated: a generated file holds only the generator's output).
#include <game/ovl/ovl.h>
#include <game/ovl/shared_classes_tr.h>

namespace rac1 {

// orig: (none) -- registers the shared classes' updates (Veldin's addresses: every level's overlay carries them)
void shared_classes_tr_register() {
    ovl_register_moby_fn(0x002a57a8, vendor_update_tr);
    ovl_register_moby_fn(0x002d5de8, infobot_update_tr);
}

}  // namespace rac1
