// The tfrag entries' static lighting (0x295350), written by hand from the routine: the same arithmetic, the same memory read and
// written in the same order, the VU0 registers as a local copy (vu0_local.h) written back at the end; as with the
// transliteration's ops (vu0_nf.h), only the op into vf0 (the point lights' range test) makes flags. The transliteration
// (world_tfrag_lighting_tr, world_lighting.cpp) stays the reference: RAC1_WORLD_LIGHTING_FAST (world_lighting_fast.cpp) picks one
// or checks one against the other.
//
// What the routine does. For each index of the list (halfwords, ended by 0xffff) the tfrag entry (0x40 bytes at *(0x160f4c)) has
// its vertices' colours made again: per vertex (8 bytes at its data + s16 entry[+0x30] + 0x10) a normal from two angle indices
// (+2: two bytes, each the index of a (cos, sin) pair of the table 0x166080, copied to SPR 0x3800 at the start), a base colour
// (+4: 1:5:5:5), and a light set (+6), lit by two directional lights:
//  - one light set for the whole entry when entry[+0x34] >= 0 (its 0x40 bytes at 0x17fec0 + 0x40 * set: colour 1 (w the back
//    factor), direction 1, colour 2, direction 2), two vertices a step;
//  - else a set per vertex: +6's low nibble names one of the 16 sets, or, when its high byte is not 0, two sets blended by its
//    weight (the second nibble the other set, the directions normalised again).
// Each light adds colour * max(n . dir, back * n . dir) to the base colour (the colours as the floats 65536 + byte / 128, so that a
// lane's low byte is the colour byte), clamped to 255 (vf22). The normals go to SPR 0x0000 (a quadword each), the colours (RGBA8)
// to the buffer at SPR 0x1000 / 0x2000 (ping-pong). Then the point lights named by the nibbles of entry[+0x36] (0x1802c0 + 0x20
// each: colour (w the floor factor), position and radius) brighten the colours in the buffer: the vertices' positions (halfwords
// at their data + the halfword at their index list, + the entry's offset) against the light's sphere. The buffer goes to the
// entry's colours (entry[+0x10] + s16 entry[+0x1e], entry[+0x29] quadwords) by fromSPR; entry[+0x35] (relight) is cleared.
// orig: (host)
#include <game/ovl/world_cull_hand.h>
#include <game/ovl/world_lighting.h>

#include <game/ovl/ovl.h>

namespace rac1::world_hand {

namespace {

constexpr uint32_t SPR = 0x70000000u;
// the scratchpad's layout during the routine
constexpr uint32_t NORMALS = SPR + 0x0000;   // the entry's normals as made, a quadword a vertex (read back by the point lights)
constexpr uint32_t SPILL = SPR + 0x0800;     // the caller's s registers (8 doublewords; the routine's own copies are zero)
constexpr uint32_t COLOURS = SPR + 0x1000;   // the colour buffer: 0x1000 and 0x2000 in turn
constexpr uint32_t ANGLES = SPR + 0x3800;    // the (cos, sin) table: 256 pairs of floats

constexpr uint32_t MAGIC = 0x47800000u;      // 65536.0f: + n is the float whose low byte is n (the colours' form)

// the VU0 registers' roles
enum : int {
    N = 1,                      // a vertex's normal (the second vertex of a step: N2)
    COLOUR1 = 27, COLOUR2 = 28,  // the lights' colours (w set to -1)
    DIR1 = 24, DIR2 = 25,        // the lights' directions
    BACK = 31,                   // the lights' back factors (x light 1, y light 2)
    ONES = 21,                   // the sum's broadcast row (0x15fb40)
    TOP = 22,                    // x: the colour's top (255)
    BASE = 30,                   // the vertex's base colour (the second vertex's: BASE2)
    N2 = 6, BASE2 = 23,
    PL_COLOUR = 29, PL_SPHERE = 26,  // a point light (colour, w the floor factor; position, w the radius)
    OFFSET = 20, POS = 10,           // the entry's offset, a vertex's position (floats of the 1/4096 fixed point)
};

struct Lighting {
    vu0l::Vu u;
    Spr spr;

    // orig: (helper) a halfword / byte of EE RAM, sign-extended (LH / LB)
    static uint32_t s16(uint32_t a) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(ee_read<uint16_t>(a)))); }
    static uint32_t s8(uint32_t a) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(ee_read<uint8_t>(a)))); }

    // orig: (helper) a vertex's normal from its angle halfword `h`: two bytes, each the index of a (cos, sin) pair (PEXTLB /
    // PEXTLH / PSLLW into the table's addresses, two LDs, PCPYLD into the register), then n.xy *= n.z, n.z = n.w, n = -n (xyz)
    void normal(int d, uint32_t h) {
        const uint32_t a = ANGLES | (h & 0xffu) << 3, b = ANGLES | ((h >> 8) & 0xffu) << 3;
        u.set_words(d, spr.u32(a), spr.u32(a + 4), spr.u32(b), spr.u32(b + 4));
        u.mulbc(d, d, u.lane(d, 2), vu0l::XY);
        u.addbc(d, 0, u.lane(d, 3), vu0l::Z);
        u.sub(d, 0, d, vu0l::XYZ);
    }
    // orig: (helper) a 1:5:5:5 colour as the four colour floats (PEXT5 / PEXTLB / PEXTLH / PADDUW with MAGIC)
    void colour5(int d, uint32_t c) {
        u.set_words(d, MAGIC + ((c & 0x1fu) << 3), MAGIC + (((c >> 5) & 0x1fu) << 3), MAGIC + (((c >> 10) & 0x1fu) << 3),
                    MAGIC + ((c & 0x8000u) != 0 ? 0x80u : 0u));
    }
    // orig: (helper) an RGBA8 word as the four colour floats (PEXTLB / PEXTLH / PADDUW)
    void colour8(int d, uint32_t c) {
        u.set_words(d, MAGIC + (c & 0xffu), MAGIC + ((c >> 8) & 0xffu), MAGIC + ((c >> 16) & 0xffu), MAGIC + (c >> 24));
    }
    // orig: (helper) the colour floats back to RGBA8: each lane's low byte (QMFC2 / PPACH / PPACB)
    uint32_t pack(int r) const {
        return (u.word(r, 0) & 0xffu) | (u.word(r, 1) & 0xffu) << 8 | (u.word(r, 2) & 0xffu) << 16 | (u.word(r, 3) & 0xffu) << 24;
    }
    // orig: (helper) LQC2 from RAM
    void load(int r, uint32_t a) { ram_load_vf(u, r, a); }

    // orig: (helper) the back factors out of the colours' w (BACK.x / .y), the colours' w set to -1
    void take_back_factors() {
        u.addbc(BACK, 0, u.lane(COLOUR1, 3), vu0l::X);
        u.addbc(BACK, 0, u.lane(COLOUR2, 3), vu0l::Y);
        u.subbc(COLOUR1, 0, u.lane(0, 3), vu0l::W);
        u.subbc(COLOUR2, 0, u.lane(0, 3), vu0l::W);
    }

    // one light set for the entry (loaded by the caller): two vertices a step, `steps` steps
    void one_set(uint32_t& vtx, uint32_t& out, uint32_t& nrm, int32_t steps) {
        using namespace vu0l;
        take_back_factors();
        do {
            const uint32_t na = s16(vtx + 0x2), ca = s16(vtx + 0x4), nb = s16(vtx + 0xa), cb = s16(vtx + 0xc);
            vtx += 0x10;
            // (both normals' table words read before the first is stored, as the routine orders them)
            normal(N, na);
            normal(N2, nb);
            colour5(BASE, ca);
            colour5(BASE2, cb);
            spr.store_vf(u, N, nrm);
            spr.store_vf(u, N2, nrm + 0x10);
            // n . dir for both lights (vf2 / vf1, vf7 / vf6: the products, then x + y + z into vf5 / vf4, vf9 / vf8)
            u.mul(2, N, DIR2, XYZ);
            u.mul(1, N, DIR1, XYZ);
            u.mul(7, N2, DIR2, XYZ);
            u.mul(6, N2, DIR1, XYZ);
            u.addabc(2, u.lane(2, 1), X);
            u.maddbc(5, ONES, u.lane(2, 2), X);
            u.addabc(1, u.lane(1, 1), X);
            u.maddbc(4, ONES, u.lane(1, 2), X);
            u.addabc(7, u.lane(7, 1), X);
            u.maddbc(9, ONES, u.lane(7, 2), X);
            u.addabc(6, u.lane(6, 1), X);
            u.maddbc(8, ONES, u.lane(6, 2), X);
            // the back faces' share, the larger kept
            u.mulbc(1, 4, u.lane(BACK, 0), X);
            u.mulbc(2, 5, u.lane(BACK, 1), X);
            u.mulbc(6, 8, u.lane(BACK, 0), X);
            u.mulbc(7, 9, u.lane(BACK, 1), X);
            u.max_lane(4, 0, u.lane(4, 0), u.lane(1, 0));
            u.max_lane(5, 0, u.lane(5, 0), u.lane(2, 0));
            u.max_lane(8, 0, u.lane(8, 0), u.lane(6, 0));
            u.max_lane(9, 0, u.lane(9, 0), u.lane(7, 0));
            // base + colour 1 * i1 + colour 2 * i2, clamped to the top
            u.mulabc(BASE, u.lane(0, 3), XYZW);
            u.maddabc(COLOUR1, u.lane(4, 0), XYZW);
            u.maddbc(N, COLOUR2, u.lane(5, 0), XYZW);
            u.mulabc(BASE2, u.lane(0, 3), XYZW);
            u.maddabc(COLOUR1, u.lane(8, 0), XYZW);
            u.maddbc(N2, COLOUR2, u.lane(9, 0), XYZW);
            clamp_top(N);
            clamp_top(N2);
            nrm += 0x20;
            spr.set32(out, pack(N));
            spr.set32(out + 4, pack(N2));
            out += 8;
        } while (--steps != 0);
    }

    // orig: (helper) VMINIx.xyz with TOP
    void clamp_top(int r) {
        const float top = u.lane(TOP, 0);
        for (int i = 0; i < 3; ++i) u.mini_lane(r, i, u.lane(r, i), top);
    }

    // a light set per vertex (the 16 sets at `sets`): one, or two blended by the vertex's weight
    void set_per_vertex(uint32_t sets, uint32_t& vtx, uint32_t& out, uint32_t& nrm, int32_t count) {
        using namespace vu0l;
        do {
            const uint32_t w = s16(vtx + 0x6);
            const uint32_t first = ((w & 0xfu) << 6) + sets;
            if ((w & 0xff00u) == 0) {
                load(COLOUR1, first);
                load(DIR1, first + 0x10);
                load(COLOUR2, first + 0x20);
                load(DIR2, first + 0x30);
            } else {
                // the weight (QMTC2 of a word: x the weight << 4, y its sign; z / w what the register's high half held, which
                // nothing reads: vf1 is written whole below before z is read, w first), x as 1/4096 fixed point, w = 1 - x
                u.vf[1].v[0] = u2f((w >> 4) & 0xff0u);
                u.vf[1].v[1] = 0.0f;
                u.itof(1, 1, X, 1.0f / 4096.0f);
                u.subbc(1, 0, u.lane(1, 0), W);
                const uint32_t second = (((w >> 4) & 0xfu) << 6) + sets;
                load(2, first);
                load(3, first + 0x10);
                load(4, first + 0x20);
                load(5, first + 0x30);
                load(6, second);
                u.mulbc(2, 2, u.lane(1, 3), XYZW);
                load(7, second + 0x10);
                u.mulbc(3, 3, u.lane(1, 3), XYZ);
                load(8, second + 0x20);
                u.mulbc(4, 4, u.lane(1, 3), XYZW);
                load(9, second + 0x30);
                u.mulbc(5, 5, u.lane(1, 3), XYZ);
                u.mulbc(7, 7, u.lane(1, 0), XYZ);
                u.mulbc(9, 9, u.lane(1, 0), XYZ);
                u.mulbc(6, 6, u.lane(1, 0), XYZW);
                u.mulbc(8, 8, u.lane(1, 0), XYZW);
                u.add(DIR1, 3, 7, XYZW);
                u.add(DIR2, 5, 9, XYZW);
                u.add(COLOUR1, 2, 6, XYZW);
                u.add(COLOUR2, 4, 8, XYZW);
                // the blended directions normalised (|d|^2 = x + y + z of d * d, Q = 1 / sqrt)
                u.mul(1, DIR1, DIR1, XYZW);
                u.addabc(1, u.lane(1, 1), X);
                u.maddbc(1, ONES, u.lane(1, 2), X);
                u.rsqrt(u.lane(0, 3), u.lane(1, 0));
                u.mul(1, DIR2, DIR2, XYZW);
                u.addabc(1, u.lane(1, 1), X);
                u.maddbc(1, ONES, u.lane(1, 2), X);
                u.mulbc(DIR1, DIR1, u.q, XYZ);
                u.rsqrt(u.lane(0, 3), u.lane(1, 0));
                u.mulbc(DIR2, DIR2, u.q, XYZ);
            }
            take_back_factors();
            normal(N, s16(vtx + 0x2));
            spr.store_vf(u, N, nrm);
            colour5(BASE, s16(vtx + 0x4));
            u.mul(2, N, DIR2, XYZ);
            u.mul(1, N, DIR1, XYZ);
            u.addabc(1, u.lane(1, 1), X);
            u.maddbc(4, ONES, u.lane(1, 2), X);
            u.addabc(2, u.lane(2, 1), X);
            u.maddbc(5, ONES, u.lane(2, 2), X);
            u.mulbc(1, 4, u.lane(BACK, 0), X);
            u.mulbc(2, 5, u.lane(BACK, 1), X);
            u.max_lane(4, 0, u.lane(4, 0), u.lane(1, 0));
            u.max_lane(5, 0, u.lane(5, 0), u.lane(2, 0));
            u.mulabc(BASE, u.lane(0, 3), XYZW);
            u.maddabc(COLOUR1, u.lane(4, 0), XYZW);
            u.maddbc(N, COLOUR2, u.lane(5, 0), XYZW);
            clamp_top(N);
            spr.set32(out, pack(N));
            nrm += 0x10;
            out += 4;
            vtx += 8;
        } while (--count != 0);
    }

    // one point light (PL_SPHERE / PL_COLOUR loaded by the caller) over the entry's vertices: the colours in the buffer at `out`
    // brightened where the vertex is inside the light's sphere. `index` the vertices' index list, `data` the entry's data.
    void point_light(uint32_t data, uint32_t index, uint32_t out, int32_t count) {
        using namespace vu0l;
        uint32_t nrm = NORMALS;
        do {
            colour8(BASE, spr.u32(out));
            {
                const uint32_t v = data + ee_read<uint16_t>(index);
                const uint32_t x = s16(v), y = s16(v + 2), z = s16(v + 4);
                // (PEXTLW / PCPYLD: w the sign word of z)
                u.set_words(POS, x << 2, y << 2, z << 2, (static_cast<int32_t>(z) < 0 ? 0xffffffffu : 0u) << 2);
                u.itof(POS, POS, XYZ, 1.0f / 4096.0f);
            }
            u.add(POS, POS, OFFSET, XYZ);
            u.div(u.lane(0, 3), u.lane(PL_SPHERE, 3));
            u.sub(2, POS, PL_SPHERE, XYZ);          // the light to the vertex
            u.mul(4, 2, 2, XYZ);
            u.mul(4, PL_SPHERE, PL_SPHERE, W);      // radius^2
            u.addabc(4, u.lane(4, 1), X);
            u.maddbc(4, ONES, u.lane(4, 2), X);     // distance^2
            u.mulbc(11, 0, u.q, W);                 // 1 / radius
            u.subbc(0, 4, u.lane(4, 0), W);         // radius^2 - distance^2: its sign (the MAC flags' w sign bit)
            const uint32_t mac = u.mac;              // (CFC2 17)
            u.sqrt(u.lane(4, 0));
            if ((mac & 0x10u) == 0) {
                u.mulbc(11, 11, u.q, W);            // distance / radius
                u.mulbc(4, 0, u.q, W);
                u.subbc(11, 0, u.lane(11, 3), W);   // 1 - that: the falloff
                u.div(u.lane(0, 3), u.lane(4, 3));
                u.mulbc(11, 11, u.q, W);
                u.mulbc(2, 2, u.lane(11, 3), XYZ);
                spr.load_vf(u, 1, nrm);             // the vertex's normal, as made above
                u.mul(1, 1, 2, XYZ);
                u.addabc(1, u.lane(1, 1), X);
                u.maddbc(4, ONES, u.lane(1, 2), X);
                u.mulbc(1, 4, u.lane(PL_COLOUR, 3), X);
                u.max_lane(4, 0, u.lane(4, 0), u.lane(1, 0));
                u.mulabc(BASE, u.lane(0, 3), XYZW);
                u.maddbc(1, PL_COLOUR, u.lane(4, 0), XYZW);
                clamp_top(1);
                spr.set32(out, pack(1));
            }
            nrm += 0x10;
            out += 4;
            index += 8;
        } while (--count != 0);
    }

    void run(uint32_t list) {
        using namespace vu0l;
        for (uint32_t k = 0; k < 8; ++k) spr.set64(SPILL + 8 * k, 0);
        const uint32_t table = lwu(OV(0x00160f4cu));
        const uint32_t sets = OV(0x0017fec0u), lights = OV(0x001802c0u);
        load(ONES, OV(0x0015fb40u));
        u.set_words(TOP, MAGIC | 0xffu, 0, 0, 0);
        dma_to_spr(ANGLES & 0x3fffu, OV(0x00166080u), 0x80);
        uint32_t buffer = COLOURS;
        for (uint32_t at = list;; at += 2) {
            const uint32_t idx = ee_read<uint16_t>(at);
            if (idx == 0xffffu) break;
            const uint32_t e = table + (idx << 6);
            const uint32_t data = lwu(e + 0x10);
            uint32_t vtx = data + s16(e + 0x30) + 0x10;
            uint32_t out = buffer, nrm = NORMALS;
            const uint32_t set = s8(e + 0x34);
            if (static_cast<int32_t>(set) >= 0) {
                const uint32_t s = sets + (set << 6);
                const int32_t steps = static_cast<int32_t>(s8(e + 0x29) << 1);
                load(COLOUR1, s);
                load(COLOUR2, s + 0x20);
                load(DIR1, s + 0x10);
                load(DIR2, s + 0x30);
                one_set(vtx, out, nrm, steps);
            } else {
                set_per_vertex(sets, vtx, out, nrm, static_cast<int32_t>(s8(e + 0x3c)));
            }
            // the point lights: the nibbles of entry[+0x36] (0xf ends)
            if (s8(e + 0x35) != 0) ee_write<uint8_t>(e + 0x35, 0);
            int32_t named = static_cast<int32_t>(ee_read<uint16_t>(e + 0x36) | 0xf0000u);
            for (;;) {
                const uint32_t n = static_cast<uint32_t>(named) & 0xfu;
                named >>= 4;
                if (n == 0xfu) break;
                const uint32_t l = (n << 5) + lights;
                load(PL_SPHERE, l + 0x10);
                load(PL_COLOUR, l);
                const uint32_t offs = data + s16(e + 0x30);
                {
                    const Qw o = ram_q(offs);
                    u.set_words(OFFSET, static_cast<uint32_t>(o.lo) << 2, static_cast<uint32_t>(o.lo >> 32) << 2, static_cast<uint32_t>(o.hi) << 2,
                                static_cast<uint32_t>(o.hi >> 32) << 2);
                    u.itof(OFFSET, OFFSET, XYZ, 1.0f / 4096.0f);
                }
                point_light(data, offs + 0x10, buffer, static_cast<int32_t>(s8(e + 0x3c)));
            }
            // the colours to the entry (fromSPR), the other buffer next
            dma_from_spr(lwu(e + 0x10) + s16(e + 0x1e), buffer & 0xffffu, s8(e + 0x29));
            buffer = SPR | ((0x2000u - (buffer & 0xffffu)) + 0x1000u);
        }
    }
};

}  // namespace

// orig: 0x00295350 -- see world_lighting.cpp's world_tfrag_lighting_tr
bool tfrag_lighting(uint32_t list) {
#if defined(RAC1_VU0_EXACT)
    (void)list;
    return false;  // (every op of the transliteration makes its flags in this build: it runs)
#else
    if (!vu0l::usable()) return false;
    Lighting l;
    l.u.load();
    l.run(list);
    l.u.store();
    return true;
#endif
}

}  // namespace rac1::world_hand
