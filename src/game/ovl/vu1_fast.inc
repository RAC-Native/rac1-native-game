// (host, 2026-10-31) The ported VU1 programs' lane rules decided on the FPU (RAC1_VU1_FAST), included by particles_vu1.cpp and
// shadows_vu1.cpp inside their anonymous namespace (a .inc, not a header: the generated code's fingerprint).
// Why: a port applies the VU's float rules (vuc: denormals the signed zero, infinities and NaNs the signed largest float) to every
// operand and result through the float's bits. On the Wii U's Espresso there is no move between the float and the integer registers:
// each vuc is a store and a load through memory, three a lane op - the chain reader's VU1 programs (Novalis: program 7's 0xe and
// 0xc, the particles' 0x14) were ~6 ms of its ~18 a cycle. Here a value's rule is decided by two float compares: a normal finite
// number is kept as it is (vuc keeps it too), anything else (a zero, a denormal, an infinity, a NaN: all compares false or the rare
// side) takes vuc itself - so always vuc's value, bit for bit. The same for the integer-coded steps the programs make on floats
// (FTOI, CLIP, MAX / MINI on the bits): their common cases on the FPU, the rest as the exact port does them.
// RAC1_VU1_FAST: 0 the exact lane rules (the PC's default), 1 the fast ones (the Wii U's), 2 testing: both run from the same state
// (the exact one on a copy), every register, flag and the data memory after them and every kicked packet compared, the differences
// printed ("[vu1-fast]").
// orig: (host)

// orig: (helper) vuc's rare cases (zeros, denormals, infinities, NaNs), out of the ops' code
[[gnu::noinline, gnu::cold]] float fvuc_rare(float x) { return vuc(x); }
// orig: (helper) vuc, its common case decided on the FPU: a normal finite number is returned as it is (vuc does too); the rest (any
// comparison of a NaN is false) by vuc itself - so always vuc's value
[[gnu::always_inline]] inline float fvuc(float x) {
    const float a = __builtin_fabsf(x);
    if (__builtin_expect(a >= 1.17549435e-38f && a <= 3.40282347e+38f, 1)) return x;
    return fvuc_rare(x);
}
// orig: (helper) a lane vuc leaves as it is and whose bits order as its value: a zero or a normal finite number (a denormal, an
// infinity, a NaN: false)
[[gnu::always_inline]] inline bool fnorm(float x) {
    const float a = __builtin_fabsf(x);
    return x == 0.0f || (a >= 1.17549435e-38f && a <= 3.40282347e+38f);
}

// orig: (host) RAC1_VU1_FAST (above): 0, 1 or 2
int vu1_fast_mode() {
    static const int mode = [] {
        const char* e = std::getenv("RAC1_VU1_FAST");
#if defined(__WIIU__)
        return e != nullptr ? std::atoi(e) : 1;
#else
        return e != nullptr ? std::atoi(e) : 0;
#endif
    }();
    return mode;
}

// orig: (host) testing (RAC1_VU1_FAST=2): `fast` run on `s` (its kicks to `kick`), `exact` on a copy of the state before it, the two
// compared - registers, Q / I, the clip flag, XTOP, the data memory, the kicked packets
template <class Exact, class Fast>
void vu1_fast_check(const char* name, VUState& s, uint32_t start, const std::function<void(uint32_t)>& kick, Exact&& exact, Fast&& fast) {
    struct Totals {
        const char* name = nullptr;
        uint64_t starts = 0, bad = 0, kicks = 0, qwords = 0;
        void print() const {
            std::printf("[vu1-fast] %s: %llu starts compared (fast lane rules against the exact port), %llu kicks, %llu qwords kicked: %llu starts differ\n", name,
                            static_cast<unsigned long long>(starts), static_cast<unsigned long long>(kicks), static_cast<unsigned long long>(qwords),
                            static_cast<unsigned long long>(bad));
        }
    };
    static Totals tot;
    tot.name = name;
    auto grab = [](const VUState& st, uint32_t addr, std::vector<std::vector<uint8_t>>& out) {
        const uint32_t qwc = vu1::gif_packet_qwc(st, addr);
        std::vector<uint8_t> b(static_cast<size_t>(qwc) * 16);
        for (uint32_t q = 0; q < qwc; ++q) std::memcpy(&b[q * 16], &st.mem[((addr + q) & 0x3ff) << 4], 16);
        out.push_back(std::move(b));
    };
    std::unique_ptr<VUState> ref(new VUState(s));
    std::vector<std::vector<uint8_t>> want, got;
    exact(*ref, start, [&](uint32_t addr) { grab(*ref, addr, want); });
    fast(s, start, [&](uint32_t addr) {
        grab(s, addr, got);
        kick(addr);
    });
    ++tot.starts;
    tot.kicks += got.size();
    for (const auto& k : got) tot.qwords += k.size() / 16;
    if (tot.starts % 5000 == 0) tot.print();  // (the totals so far: the process may end without its destructors)
    const VUState& a = *ref;
    const char* what = nullptr;
    if (std::memcmp(a.vf.data(), s.vf.data(), sizeof(VUVector) * 32) != 0) what = "vf";
    else if (std::memcmp(a.vi.data(), s.vi.data(), sizeof(int16_t) * 16) != 0) what = "vi";
    else if (std::memcmp(&a.acc, &s.acc, sizeof(VUVector)) != 0) what = "ACC";
    else if (std::memcmp(&a.q, &s.q, 4) != 0 || std::memcmp(&a.i, &s.i, 4) != 0 || std::memcmp(&a.r, &s.r, 4) != 0 || std::memcmp(&a.p, &s.p, 4) != 0) what = "Q / I / R / P";
    else if (a.clipflag != s.clipflag || a.macflag != s.macflag || a.statusflag != s.statusflag) what = "flags";
    else if (a.xtop != s.xtop) what = "XTOP";
    else if (a.mem != s.mem) what = "data memory";
    else if (want != got) what = "kicks";
    if (what == nullptr) return;
    ++tot.bad;
    tot.print();
    static int shown = 0;
    if (shown++ < 8) {
        std::printf("[vu1-fast] %s start %03x: the fast lane rules differ from the exact port in %s", name, start, what);
        if (what[0] == 'v' && what[1] == 'f')
            for (int r = 0; r < 32; ++r)
                if (std::memcmp(&a.vf[static_cast<size_t>(r)], &s.vf[static_cast<size_t>(r)], 16) != 0) {
                    std::printf(" (vf%d exact %08x %08x %08x %08x fast %08x %08x %08x %08x)", r, a.vf[r].u[0], a.vf[r].u[1], a.vf[r].u[2], a.vf[r].u[3], s.vf[r].u[0], s.vf[r].u[1],
                                s.vf[r].u[2], s.vf[r].u[3]);
                    break;
                }
        std::printf("\n");
    }
}
