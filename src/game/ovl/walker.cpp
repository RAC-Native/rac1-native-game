// The ground enemies' stepping, see walker.h.
#include <game/ovl/walker.h>

#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// Whether the way from `pos` up by `up` to `pos` down by `down` (its z at least 0) is blocked: a segment
// test (flags 0x22, ignoring `self`) that counts only when it hit something (0x173e5c, the hit
// record's id, is not below 0) and, with `no_moby`, no moby (0x173e58 is 0).
// orig: 0x002583f0
int32_t walker_probe(float up, float down, uint32_t pos, uint32_t self, uint32_t no_moby) {
    Vec4 start = V(pos);
    start.z = start.z + up;
    Vec4 end = V(pos);
    float z = end.z - down;
    end.z = z;
    if (z < 0.0f) end.z = 0.0f;
    int32_t r = segment_collide_v(start, end, 0x22, self, nullptr);
    if (!(-1 < coll_hit_flags_signed())) r = 0;
    if (no_moby != 0 && coll_hit_moby() != 0) r = 0;
    return r;
}

// One step of a walker `m` (record `rec`, 0x38 flags: 1 no gravity, 4 own turn angle +0x4c, 8 and 0x20 how it
// treats ledges / mobys, 0x2c the stop distance): its speed (+0x10) grows toward the distance to `target`
// (acceleration +0x1c * `scale`, at most the top speed +0x24 * `scale`, drag +0x20 once close), a
// velocity along its heading (or the turn angle), gravity on +0x18; the step is probed (walker_probe,
// and a short probe ahead against walls and ledges): a blocked step stops it (flags 0x22 or 8), a free one
// moves it (walker_move_fx) and is put back when the floor is further than +8 / +0xc away; the velocity goes to
// `out`. Returns flags: 1 touched something, 2 blocked / came off the floor, 4 near the target, 8 / 0x20.
// orig: 0x00258b50
uint32_t walker_record_step(float scale, uint32_t m, uint32_t rec, uint32_t target, uint32_t out) {
    uint32_t flags = 0;
    const float accel = lwc1(rec + 0x1c) * scale;
    const float vmax = lwc1(rec + 0x24) * scale;
    const uint32_t buf = ee_stack_alloc(0x60);
    const uint32_t home = buf, vel = buf + 0x10, cand = buf + 0x20, cand2 = buf + 0x30;
    vec3_sub(V(vel), V(target), V(m + 0x10));
    const float dist = vec2_length(V(vel));
    V(home) = V(m + 0x10);
    float step = accel * dist;
    if (accel < step) step = accel;
    float speed = lwc1(rec + 0x10) + step;
    swc1(rec + 0x10, speed);
    if (dist < speed * 10.0f) swc1(rec + 0x10, speed - lwc1(rec + 0x20) * speed);
    if (vmax < lwc1(rec + 0x10)) swc1(rec + 0x10, vmax);
    else if (lwc1(rec + 0x10) < -vmax) swc1(rec + 0x10, -vmax);
    const float heading = (lw(rec + 0x38) & 4) != 0 ? lwc1(rec + 0x4c) : lwc1(m + 0x48);
    swc1(vel, cos_vu(heading) * lwc1(rec + 0x10));
    swc1(vel + 4, sin_vu(heading) * lwc1(rec + 0x10));
    if ((lw(rec + 0x38) & 1) != 0) {
        sw(vel + 8, 0);
    } else {
        const float g = lwc1(0x0015ed70) * 9.0f;
        const float vz = lwc1(rec + 0x18);
        swc1(vel + 8, vz);
        swc1(rec + 0x18, vz - g);
    }
    const float top = lwc1(m + 0x18) + u2f(0x3d23d70au);
    vec3_add(V(cand), V(m + 0x10), V(vel));
    swc1(cand + 8, lwc1(m + 0x18) - lwc1(rec + 0x30));
    uint32_t blocked_or = 0;
    if (walker_probe(lwc1(rec + 8), lwc1(rec + 0xc), cand, m, static_cast<uint32_t>(lw(rec + 0x38)) & 0x20u) == 0) {
        flags = (lw(rec + 0x38) & 8) != 0 ? 8u : 0x22u;
        blocked_or = flags & 2u;
    } else {
        const float z1 = coll_hit_point().z;
        if (0.5f < atan2_f(coll_hit_normal().z, vec2_length(coll_hit_normal())) && u2f(0x3dcccccdu) < vec2_length(V(vel))) {
            vec3_set_length(V(cand2), V(vel), u2f(0x3f11eb85u));
            vec3_add(V(cand2), V(cand2), V(m + 0x10));
            swc1(cand2 + 8, lwc1(m + 0x18) - lwc1(rec + 0x30));
            if (walker_probe(lwc1(rec + 8), lwc1(rec + 0xc), cand2, m, static_cast<uint32_t>(lw(rec + 0x38)) & 0x20u) == 0) {
                flags = 0x22;
                if ((lw(rec + 0x38) & 8) != 0 && z1 < top) flags = 8;
            } else {
                const float z2 = coll_hit_point().z;
                if (!(0.5f <= atan2_f(coll_hit_normal().z, vec2_length(coll_hit_normal())))) {
                    // (flags stay 0)
                } else if ((lw(rec + 0x38) & 8) == 0) {
                    flags = 0x22;
                } else {
                    flags = 0x22;
                    if (z2 < top) {
                        if (z1 < top) flags = 8;
                    }
                }
            }
        }
        blocked_or = flags & 2u;
    }
    float stop;
    if (blocked_or != 0) {
        if (((lw(rec + 0x38) ^ 1) & 1) != 0) swc1(rec + 0x18, lwc1(0x0015ed60) * u2f(0xbc23d70au));
        stop = lwc1(rec + 0x2c);
    } else {
        const float len = vec2_length(V(vel));
        vec3_scale(V(cand), V(vel), fpu_div(1.0f, len));
        const uint32_t moved = walker_move_fx(m, vel, lw(rec), static_cast<uint32_t>(lw(rec + 0x38)), lwc1(rec + 4), lwc1(rec + 0x30));
        if ((moved & 2u) != 0) swc1(rec + 0x18, lwc1(0x0015ed60) * u2f(0xbc23d70au));
        if ((moved & 1u) != 0) {
            const float along = lwc1(vel) * lwc1(cand) + lwc1(vel + 4) * lwc1(cand + 4);
            flags |= 1;
            if (along < len * u2f(0x3e4ccccdu)) flags |= 2;
            const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
            const float saved_z = lwc1(home + 8);
            if (lwc1(rec + 8) < saved_z - floor || lwc1(rec + 0xc) < floor - saved_z) {
                flags |= 0x22;
                V(m + 0x10) = V(home);
            }
        }
        V(out) = V(vel);
        stop = lwc1(rec + 0x2c);
    }
    if (dist < stop) flags |= 4;
    ee_stack_free(0x60);
    return flags;
}

}  // namespace rac1
