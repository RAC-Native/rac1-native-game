// The registration of particle types 0x47 and 0x4f's updates (formerly particle_types4_tr.cpp, see particle_types4_tr.h).
#include <game/ovl/particle_types4_tr.h>

#include <game/ovl/ovl.h>

namespace rac1 {

// orig: (none) -- see particle_types4_tr.h
void particle_types4_register() {
    ovl_register_moby_fn(0x00274e48, particle_47_update);  // (particle type 0x47: L17:0x26eae0)
    ovl_register_moby_fn(0x00275d68, particle_4f_update);  // (particle type 0x4f: L17:0x26fc88)
}

}  // namespace rac1
