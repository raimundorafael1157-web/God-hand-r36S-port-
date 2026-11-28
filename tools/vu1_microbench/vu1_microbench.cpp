// vu1_microbench: isolated microbenchmarks of the operations the pre-compiled VU1 code runs, for the PC (native SSE) and
// the RK3326 (Cortex-A35, the same SSE code translated by sse2neon v1.8.0, as in the game build).
//
// Measurement only. It does not link the game or the runtime: the helpers below are copies of the ones in
// runtime/ps2xRuntime/src/lib/vu/ps2_vu1_impl.inl (godhand-recomp, GPL-3.0), reduced to what each test needs.
//
// Each test runs one "op" per step, unrolled 8x with constant register numbers (as the generated code has them),
// on state kept in memory (as the generated code keeps vf/acc/norm/ready in the VU1 objects). Throughput tests
// (thr) use independent steps; latency tests (lat) chain each result into the next step.
//
// Output, one line per test: median/min/max ns per op over the repetitions, and, when perf_event_open works, CPU
// cycles and instructions per op (else cycles are estimated from the clock read in sysfs, marked "est").
//
// Usage: vu1_microbench [--cpu N] [--reps 7] [--ms 250] [--only name] [--csv file]

#if defined(__aarch64__) || defined(__arm__)
#include "sse2neon.h"
#define MB_ARCH "aarch64 (sse2neon)"
#else
#include <immintrin.h>
#define MB_ARCH "x86-64 (SSE)"
#endif

#include <algorithm>
#include <cerrno>
#include <cfloat>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <vector>

#include <linux/perf_event.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#define NOINLINE __attribute__((noinline))
#if defined(__aarch64__)
#define VREG "w"
#else
#define VREG "x"
#endif
#define INLINE __attribute__((always_inline)) inline

// ---------------------------------------------------------------------------------------------------------------
// State, laid out like the VU1 objects the generated code touches.

struct alignas(64) FlagRecord
{
    uint64_t ready;
    uint32_t mac, status, extra, clip, writes, pad;
};

struct alignas(64) VuState
{
    float vf[32][4];
    float acc[4];
    alignas(16) float norm[33][4];
    uint64_t vfReady[32][4];
    uint64_t viReady[16];
    int32_t vi[16];
    int32_t viBackupValue;
    uint32_t viBackupReg;
    bool viBackupValid;
    uint32_t flagHead, flagTail;
    FlagRecord flags[32];
    alignas(64) uint8_t mem[16384];
    uint32_t mac, status;
};

alignas(16) static constexpr uint32_t kDestMaskBits[16][4] = {
    {0u, 0u, 0u, 0u}, {0u, 0u, 0u, ~0u}, {0u, 0u, ~0u, 0u}, {0u, 0u, ~0u, ~0u},
    {0u, ~0u, 0u, 0u}, {0u, ~0u, 0u, ~0u}, {0u, ~0u, ~0u, 0u}, {0u, ~0u, ~0u, ~0u},
    {~0u, 0u, 0u, 0u}, {~0u, 0u, 0u, ~0u}, {~0u, 0u, ~0u, 0u}, {~0u, 0u, ~0u, ~0u},
    {~0u, ~0u, 0u, 0u}, {~0u, ~0u, 0u, ~0u}, {~0u, ~0u, ~0u, 0u}, {~0u, ~0u, ~0u, ~0u}};
static constexpr uint8_t kRev4[16] = {0x0u, 0x8u, 0x4u, 0xCu, 0x2u, 0xAu, 0x6u, 0xEu,
                                      0x1u, 0x9u, 0x5u, 0xDu, 0x3u, 0xBu, 0x7u, 0xFu};
static constexpr uint64_t kFmacLatency = 4u;

// ---- copies of the runtime helpers (ps2_vu1_impl.inl) ----

static INLINE uint64_t rmax(uint64_t a, uint64_t b) { return a < b ? b : a; }

static INLINE __m128 destMask(uint32_t dest) { return _mm_load_ps(reinterpret_cast<const float *>(kDestMaskBits[dest])); }

static INLINE __m128 blendDest(__m128 old, __m128 value, uint32_t dest)
{
    if (dest == 0xFu)
        return value;
    const __m128 mask = destMask(dest);
    return _mm_or_ps(_mm_and_ps(mask, value), _mm_andnot_ps(mask, old));
}

static INLINE void storeDest(float *target, __m128 value, uint32_t dest)
{
    if (dest == 0xFu)
        _mm_storeu_ps(target, value);
    else
        _mm_storeu_ps(target, blendDest(_mm_loadu_ps(target), value, dest));
}

static INLINE __m128 vnorm(__m128 value)
{
    const __m128i bits = _mm_castps_si128(value);
    const __m128i exponent = _mm_and_si128(bits, _mm_set1_epi32(0x7F800000));
    const __m128i sign = _mm_and_si128(bits, _mm_set1_epi32(static_cast<int>(0x80000000u)));
    const __m128i zero = _mm_cmpeq_epi32(exponent, _mm_setzero_si128());
    const __m128i huge = _mm_cmpeq_epi32(exponent, _mm_set1_epi32(0x7F800000));
    __m128i result = _mm_or_si128(_mm_andnot_si128(zero, bits), _mm_and_si128(zero, sign));
    result = _mm_or_si128(_mm_andnot_si128(huge, result), _mm_and_si128(huge, _mm_or_si128(sign, _mm_set1_epi32(0x7F7FFFFF))));
    return _mm_castsi128_ps(result);
}

static INLINE __m128 nv(VuState &s, uint32_t reg) { return _mm_load_ps(s.norm[reg]); }
static INLINE __m128 nbc(VuState &s, uint32_t reg, uint32_t bc) { return _mm_set1_ps(s.norm[reg][bc]); }
static INLINE __m128 nacc(VuState &s) { return _mm_load_ps(s.norm[32]); }
static INLINE float *rawReg(VuState &s, uint32_t reg) { return reg < 32u ? s.vf[reg] : s.acc; }

static INLINE void writeVf(VuState &s, uint32_t reg, __m128 raw, __m128 normalized, uint32_t dest)
{
    storeDest(rawReg(s, reg), raw, dest);
    if (dest == 0xFu)
        _mm_store_ps(s.norm[reg], normalized);
    else
        _mm_store_ps(s.norm[reg], blendDest(_mm_load_ps(s.norm[reg]), normalized, dest));
}

static INLINE void setReady4(uint64_t *ready, uint64_t value)
{
    ready[0] = value;
    ready[1] = value;
    ready[2] = value;
    ready[3] = value;
}

static INLINE uint32_t qaddr(int32_t base, int32_t imm) { return (static_cast<uint32_t>(base + imm) * 16u) & (16384u - 1u); }
static INLINE __m128 ldq(const uint8_t *mem, uint32_t address) { return _mm_loadu_ps(reinterpret_cast<const float *>(mem + address)); }

NOINLINE static void makeStatusRoom(VuState &s)
{
    // The runtime folds the oldest records into the flag registers; here: fold one record.
    FlagRecord &r = s.flags[s.flagHead++ & 31u];
    s.mac = r.mac;
    s.status = (s.status & 0xFF0u) | (r.status & 0xFu);
}

static INLINE FlagRecord &statusPush(VuState &s, uint64_t cycle)
{
    if (s.flagTail - s.flagHead == 32u)
        makeStatusRoom(s);
    FlagRecord &record = s.flags[s.flagTail++ & 31u];
    record.ready = cycle + kFmacLatency;
    record.mac = 0u;
    record.status = 0u;
    record.extra = 0u;
    record.clip = 0u;
    record.writes = 0u;
    return record;
}

static INLINE void pushFmacFlags(VuState &s, uint64_t cycle, uint32_t mac, uint32_t status, uint32_t extra)
{
    FlagRecord &record = statusPush(s, cycle);
    record.mac = mac;
    record.status = status;
    record.extra = extra;
    record.writes = 3u;
}

static INLINE uint32_t maskBits(__m128d lo, __m128d hi)
{
    return kRev4[static_cast<uint32_t>(_mm_movemask_pd(lo)) | (static_cast<uint32_t>(_mm_movemask_pd(hi)) << 2)];
}

struct FmacFlags
{
    uint32_t mac = 0, status = 0, extra = 0;
};

struct FmacCore
{
    __m128 s, t, result, product, resultZero, productZero;
    bool safe;
};

// fmacCore<Madd> (the fast path of MADD; ADD/MUL are subsets of it).
static INLINE FmacCore fmacCoreMadd(uint32_t dest, __m128 s, __m128 t, __m128 a)
{
    const __m128 product = _mm_mul_ps(s, t);
    const __m128 result = _mm_add_ps(a, product);
    const __m128 absMask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    const __m128 zero = _mm_setzero_ps();
    const __m128 absResult = _mm_and_ps(result, absMask);
    const __m128 resultRange = _mm_and_ps(_mm_cmpge_ps(absResult, _mm_set1_ps(4.0f * std::numeric_limits<float>::min())),
                                          _mm_cmplt_ps(absResult, _mm_set1_ps(0.5f * std::numeric_limits<float>::max())));
    const __m128 resultZero = _mm_cmpeq_ps(result, zero);
    __m128 safe = resultRange;
    const __m128 productZero = _mm_or_ps(_mm_cmpeq_ps(s, zero), _mm_cmpeq_ps(t, zero));
    const __m128 absProduct = _mm_and_ps(product, absMask);
    safe = _mm_and_ps(safe, _mm_cmpgt_ps(absResult, _mm_mul_ps(absProduct, _mm_set1_ps(1.0f / 2097152.0f))));
    safe = _mm_and_ps(safe, _mm_and_ps(_mm_cmpge_ps(absProduct, _mm_set1_ps(std::numeric_limits<float>::min())),
                                       _mm_cmplt_ps(absProduct, _mm_set1_ps(std::numeric_limits<float>::max()))));
    safe = _mm_or_ps(safe, productZero);
    const uint32_t destLanes = kRev4[dest];
    return {s, t, result, product, resultZero, productZero, (static_cast<uint32_t>(_mm_movemask_ps(safe)) & destLanes) == destLanes};
}

// fmacExact<Madd>: the double-precision path, taken when the fast one is not "safe" (NOINLINE, as in the runtime).
NOINLINE static __m128 fmacExactMadd(uint32_t dest, __m128 s, __m128 t, __m128 a, __m128 result, __m128 &normalized, FmacFlags &flags)
{
    const __m128d sLo = _mm_cvtps_pd(s);
    const __m128d sHi = _mm_cvtps_pd(_mm_movehl_ps(s, s));
    const __m128d tLo = _mm_cvtps_pd(t);
    const __m128d tHi = _mm_cvtps_pd(_mm_movehl_ps(t, t));
    const __m128d pLo = _mm_mul_pd(sLo, tLo);
    const __m128d pHi = _mm_mul_pd(sHi, tHi);
    const __m128d aLo = _mm_cvtps_pd(a);
    const __m128d aHi = _mm_cvtps_pd(_mm_movehl_ps(a, a));
    const __m128d eLo = _mm_add_pd(aLo, pLo);
    const __m128d eHi = _mm_add_pd(aHi, pHi);

    const __m128d absMask = _mm_castsi128_pd(_mm_set1_epi64x(0x7FFFFFFFFFFFFFFFll));
    const __m128d maxD = _mm_set1_pd(static_cast<double>(std::numeric_limits<float>::max()));
    const __m128d minD = _mm_set1_pd(static_cast<double>(std::numeric_limits<float>::min()));
    const __m128d abLo = _mm_and_pd(eLo, absMask);
    const __m128d abHi = _mm_and_pd(eHi, absMask);
    const __m128d zLo = _mm_cmpeq_pd(abLo, _mm_setzero_pd());
    const __m128d zHi = _mm_cmpeq_pd(abHi, _mm_setzero_pd());
    const __m128d oLo = _mm_cmpgt_pd(abLo, maxD);
    const __m128d oHi = _mm_cmpgt_pd(abHi, maxD);
    const __m128d smLo = _mm_cmplt_pd(abLo, minD);
    const __m128d smHi = _mm_cmplt_pd(abHi, minD);

    const __m128 smallF = _mm_shuffle_ps(_mm_castpd_ps(smLo), _mm_castpd_ps(smHi), _MM_SHUFFLE(2, 0, 2, 0));
    const __m128 overF = _mm_shuffle_ps(_mm_castpd_ps(oLo), _mm_castpd_ps(oHi), _MM_SHUFFLE(2, 0, 2, 0));
    const __m128 signF = _mm_and_ps(_mm_shuffle_ps(_mm_castpd_ps(eLo), _mm_castpd_ps(eHi), _MM_SHUFFLE(3, 1, 3, 1)),
                                    _mm_castsi128_ps(_mm_set1_epi32(static_cast<int>(0x80000000u))));
    result = _mm_or_ps(_mm_andnot_ps(smallF, result), _mm_and_ps(smallF, signF));
    result = _mm_or_ps(_mm_andnot_ps(overF, result), _mm_and_ps(overF, _mm_or_ps(signF, _mm_castsi128_ps(_mm_set1_epi32(0x7F7FFFFF)))));
    if (dest != 0u)
    {
        const uint32_t zeroBits = maskBits(zLo, zHi);
        const uint32_t smallBits = maskBits(smLo, smHi) & dest;
        const uint32_t signBits = maskBits(eLo, eHi) & dest;
        const uint32_t underBits = smallBits & ~zeroBits;
        const uint32_t overBits = maskBits(oLo, oHi) & dest;
        flags.mac = smallBits | (signBits << 4) | (underBits << 8) | (overBits << 12);
        flags.status = (smallBits ? 1u : 0u) | (signBits ? 2u : 0u) | (underBits ? 4u : 0u) | (overBits ? 8u : 0u);
        const __m128d paLo = _mm_and_pd(pLo, absMask);
        const __m128d paHi = _mm_and_pd(pHi, absMask);
        const uint32_t pZero = maskBits(_mm_cmpeq_pd(paLo, _mm_setzero_pd()), _mm_cmpeq_pd(paHi, _mm_setzero_pd()));
        const uint32_t pSmall = maskBits(_mm_cmplt_pd(paLo, minD), _mm_cmplt_pd(paHi, minD)) & dest;
        const uint32_t pSign = maskBits(pLo, pHi) & dest;
        const uint32_t pOver = maskBits(_mm_cmpgt_pd(paLo, maxD), _mm_cmpgt_pd(paHi, maxD)) & dest;
        flags.extra = (pSmall ? 1u : 0u) | (pSign ? 2u : 0u) | ((pSmall & ~pZero) ? 4u : 0u) | (pOver ? 8u : 0u);
    }
    normalized = vnorm(result);
    return result;
}

// fmacF<Madd>: fast path, or the exact one when not safe (forceExact: test hook to set the share of exact calls).
static INLINE __m128 fmacMadd(VuState &st, uint64_t cycle, uint32_t dest, __m128 s, __m128 t, __m128 a, __m128 &normalized, bool forceExact)
{
    FmacFlags flags;
    const FmacCore c = fmacCoreMadd(dest, s, t, a);
    __m128 result;
    if (!c.safe || forceExact)
        result = fmacExactMadd(dest, c.s, c.t, a, c.result, normalized, flags);
    else
    {
        if (dest != 0u)
        {
            const uint32_t destLanes = kRev4[dest];
            const uint32_t sign = kRev4[_mm_movemask_ps(c.result)] & dest;
            const uint32_t zeroBits = kRev4[_mm_movemask_ps(c.resultZero)] & dest;
            uint32_t extra = (static_cast<uint32_t>(_mm_movemask_ps(c.product)) & destLanes) != 0u ? 2u : 0u;
            if ((static_cast<uint32_t>(_mm_movemask_ps(c.productZero)) & destLanes) != 0u)
                extra |= 1u;
            flags.mac = zeroBits | (sign << 4);
            flags.status = (zeroBits != 0u ? 1u : 0u) | (sign != 0u ? 2u : 0u);
            flags.extra = extra;
        }
        normalized = c.result;
        result = c.result;
    }
    if (dest != 0u)
        pushFmacFlags(st, cycle, flags.mac, flags.status, flags.extra);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------
// Tests. Each returns a value derived from its work (kept in g_sink) and runs `steps` ops.

static volatile uint64_t g_sink;
static volatile uint64_t g_budget = ~0ull >> 1; // read once per test: the compiler cannot drop the checks
static VuState *g_st;
static uint8_t g_force[256]; // forceExact pattern for the mixed tests

static INLINE uint64_t fold(__m128 v)
{
    uint32_t w[4];
    _mm_storeu_ps(reinterpret_cast<float *>(w), v);
    return w[0] ^ w[1] ^ w[2] ^ w[3];
}

// Empty loop with a compiler barrier: the per-step floor of the harness.
NOINLINE static uint64_t t_loop(uint64_t steps)
{
    uint64_t x = 0;
    for (uint64_t i = 0; i < steps; i += 8)
    {
        x += i;
        asm volatile("" : "+r"(x));
    }
    return x;
}

// Cycle bookkeeping without stalls: the "if (cyc >= budget)" check and ++cyc of every VU instruction.
NOINLINE static uint64_t t_budget(uint64_t steps)
{
    VuState &s = *g_st;
    uint64_t cyc = 0;
    const uint64_t budget = g_budget; // a run-time value, as ctx.budget
    uint64_t exits = 0;
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP                     \
    if (cyc >= budget) { ++exits; break; } \
    ++cyc;
        STEP STEP STEP STEP STEP STEP STEP STEP
#undef STEP
        asm volatile("" : "+r"(cyc));
    }
    return cyc + exits + s.vi[0];
}

// Budget check + two rmax stall computations against ready times in memory + the ready write of the destination
// (the integer-register pattern of the generated code: "r = rmax(r, viReady[a]); r = rmax(r, viReady[b])").
NOINLINE static uint64_t t_rmax2(uint64_t steps)
{
    VuState &s = *g_st;
    uint64_t cyc = 64;
    const uint64_t budget = g_budget; // a run-time value, as ctx.budget
#define STEP(A, B, D)                                                                         \
    if (cyc >= budget) break;                                                                 \
    {                                                                                         \
        uint64_t r = cyc;                                                                     \
        r = rmax(r, s.viReady[A]);                                                            \
        r = rmax(r, s.viReady[B]);                                                            \
        if (r > cyc) { if (r >= budget) { cyc = budget; break; } cyc = r; }                   \
    }                                                                                         \
    s.viReady[D] = cyc + 1u;                                                                  \
    ++cyc;
    for (uint64_t i = 0; i < steps; i += 8)
    {
        STEP(1, 2, 3) STEP(4, 5, 6) STEP(7, 8, 9) STEP(10, 11, 12) STEP(13, 14, 15) STEP(1, 3, 5) STEP(2, 4, 6) STEP(7, 9, 11)
    }
#undef STEP
    return cyc;
}

// The same with a 4-lane VF ready check (rmax over 4 lanes of two source registers), as the FMAC instructions do.
NOINLINE static uint64_t t_rmaxvf(uint64_t steps)
{
    VuState &s = *g_st;
    uint64_t cyc = 64;
    const uint64_t budget = g_budget; // a run-time value, as ctx.budget
#define STEP(A, B, D)                                                                                     \
    if (cyc >= budget) break;                                                                             \
    {                                                                                                     \
        uint64_t r = cyc;                                                                                 \
        r = rmax(r, s.vfReady[A][0]); r = rmax(r, s.vfReady[A][1]); r = rmax(r, s.vfReady[A][2]); r = rmax(r, s.vfReady[A][3]); \
        r = rmax(r, s.vfReady[B][0]); r = rmax(r, s.vfReady[B][1]); r = rmax(r, s.vfReady[B][2]); r = rmax(r, s.vfReady[B][3]); \
        if (r > cyc) { if (r >= budget) { cyc = budget; break; } cyc = r; }                               \
    }                                                                                                     \
    setReady4(s.vfReady[D], cyc + kFmacLatency);                                                          \
    ++cyc;
    for (uint64_t i = 0; i < steps; i += 8)
    {
        STEP(1, 2, 3) STEP(4, 5, 6) STEP(7, 8, 9) STEP(10, 11, 12) STEP(13, 14, 15) STEP(16, 17, 18) STEP(19, 20, 21) STEP(22, 23, 24)
    }
#undef STEP
    return cyc;
}

// vnorm, throughput: 8 independent registers per step block, from memory to memory.
NOINLINE static uint64_t t_vnorm_thr(uint64_t steps)
{
    VuState &s = *g_st;
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP(R) _mm_store_ps(s.norm[R], vnorm(_mm_load_ps(s.vf[R])));
        STEP(1) STEP(2) STEP(3) STEP(4) STEP(5) STEP(6) STEP(7) STEP(8)
#undef STEP
        asm volatile("" ::: "memory");
    }
    return fold(_mm_load_ps(s.norm[3]));
}

// vnorm, latency: each result feeds the next vnorm (kept in a register).
NOINLINE static uint64_t t_vnorm_lat(uint64_t steps)
{
    __m128 x = _mm_load_ps(g_st->vf[1]);
    for (uint64_t i = 0; i < steps; i += 8)
    {
        x = vnorm(x); x = vnorm(x); x = vnorm(x); x = vnorm(x);
        x = vnorm(x); x = vnorm(x); x = vnorm(x); x = vnorm(x);
        asm volatile("" : "+" VREG(x));
    }
    return fold(x);
}

// writeVf with a partial dest (blend of raw and normalized copies), the store side of every VF write.
NOINLINE static uint64_t t_writevf(uint64_t steps)
{
    VuState &s = *g_st;
    const __m128 v = _mm_set_ps(1.0f, 2.0f, 3.0f, 4.0f);
    for (uint64_t i = 0; i < steps; i += 8)
    {
        writeVf(s, 1, v, v, 0xE); writeVf(s, 2, v, v, 0x8); writeVf(s, 3, v, v, 0x7); writeVf(s, 4, v, v, 0xF);
        writeVf(s, 5, v, v, 0xC); writeVf(s, 6, v, v, 0x1); writeVf(s, 7, v, v, 0xE); writeVf(s, 8, v, v, 0xF);
        asm volatile("" ::: "memory");
    }
    return fold(_mm_load_ps(s.norm[3]));
}

// VU memory: qaddr + ldq + storeDest (LQ/SQ), addresses from integer registers in memory.
NOINLINE static uint64_t t_ldst(uint64_t steps)
{
    VuState &s = *g_st;
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP(B, IMM, DST) \
    { const __m128 v = ldq(s.mem, qaddr(s.vi[B], IMM)); storeDest(reinterpret_cast<float *>(s.mem + qaddr(s.vi[B], IMM + 64)), v, DST); }
        STEP(1, 0, 0xF) STEP(2, 1, 0xE) STEP(3, 2, 0xF) STEP(4, 3, 0x8) STEP(1, 4, 0xF) STEP(2, 5, 0xF) STEP(3, 6, 0x7) STEP(4, 7, 0xF)
#undef STEP
        asm volatile("" ::: "memory");
    }
    return s.mem[100];
}

// movemask + kRev4 table, throughput (independent).
NOINLINE static uint64_t t_movemask_thr(uint64_t steps)
{
    VuState &s = *g_st;
    uint32_t acc = 0;
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP(R) acc += kRev4[_mm_movemask_ps(_mm_load_ps(s.vf[R]))] & 0xEu;
        STEP(1) STEP(2) STEP(3) STEP(4) STEP(5) STEP(6) STEP(7) STEP(8)
#undef STEP
        asm volatile("" : "+r"(acc) :: "memory");
    }
    return acc;
}

// movemask, latency of the NEON -> general register -> NEON round trip (the mask decides the next vector).
NOINLINE static uint64_t t_movemask_lat(uint64_t steps)
{
    __m128 x = _mm_set_ps(-1.0f, 2.0f, -3.0f, 4.0f);
    const __m128 *tbl = reinterpret_cast<const __m128 *>(kDestMaskBits);
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP x = _mm_xor_ps(x, _mm_load_ps(reinterpret_cast<const float *>(&tbl[kRev4[_mm_movemask_ps(x)] & 0x3u])));
        STEP STEP STEP STEP STEP STEP STEP STEP
#undef STEP
        asm volatile("" : "+" VREG(x));
    }
    return fold(x);
}

// Plain float math (mul + add), latency chain: the raw arithmetic floor of an FMAC.
NOINLINE static uint64_t t_muladd_lat(uint64_t steps)
{
    __m128 x = _mm_set1_ps(1.0f);
    const __m128 m = _mm_set1_ps(0.9999f), a = _mm_set1_ps(0.0001f);
    for (uint64_t i = 0; i < steps; i += 8)
    {
        x = _mm_add_ps(_mm_mul_ps(x, m), a); x = _mm_add_ps(_mm_mul_ps(x, m), a);
        x = _mm_add_ps(_mm_mul_ps(x, m), a); x = _mm_add_ps(_mm_mul_ps(x, m), a);
        x = _mm_add_ps(_mm_mul_ps(x, m), a); x = _mm_add_ps(_mm_mul_ps(x, m), a);
        x = _mm_add_ps(_mm_mul_ps(x, m), a); x = _mm_add_ps(_mm_mul_ps(x, m), a);
        asm volatile("" : "+" VREG(x));
    }
    return fold(x);
}

// Plain float math, throughput (8 independent chains).
NOINLINE static uint64_t t_muladd_thr(uint64_t steps)
{
    __m128 x0 = _mm_set1_ps(1.0f), x1 = x0, x2 = x0, x3 = x0, x4 = x0, x5 = x0, x6 = x0, x7 = x0;
    const __m128 m = _mm_set1_ps(0.9999f), a = _mm_set1_ps(0.0001f);
    for (uint64_t i = 0; i < steps; i += 8)
    {
        x0 = _mm_add_ps(_mm_mul_ps(x0, m), a); x1 = _mm_add_ps(_mm_mul_ps(x1, m), a);
        x2 = _mm_add_ps(_mm_mul_ps(x2, m), a); x3 = _mm_add_ps(_mm_mul_ps(x3, m), a);
        x4 = _mm_add_ps(_mm_mul_ps(x4, m), a); x5 = _mm_add_ps(_mm_mul_ps(x5, m), a);
        x6 = _mm_add_ps(_mm_mul_ps(x6, m), a); x7 = _mm_add_ps(_mm_mul_ps(x7, m), a);
        asm volatile("" : "+" VREG(x0), "+" VREG(x1), "+" VREG(x2), "+" VREG(x3), "+" VREG(x4), "+" VREG(x5), "+" VREG(x6), "+" VREG(x7));
    }
    return fold(_mm_add_ps(_mm_add_ps(_mm_add_ps(x0, x1), _mm_add_ps(x2, x3)), _mm_add_ps(_mm_add_ps(x4, x5), _mm_add_ps(x6, x7))));
}

// fmacCore<Madd> only (math + safety checks + one movemask), registers from memory, result to memory.
NOINLINE static uint64_t t_fmaccore(uint64_t steps)
{
    VuState &s = *g_st;
    uint32_t safeCount = 0;
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP(A, B, D) { const FmacCore c = fmacCoreMadd(0xF, nv(s, A), nbc(s, B, 1), nacc(s)); safeCount += c.safe; _mm_store_ps(s.norm[D], c.result); }
        STEP(1, 2, 9) STEP(3, 4, 10) STEP(5, 6, 11) STEP(7, 8, 12) STEP(1, 3, 13) STEP(2, 4, 14) STEP(5, 7, 15) STEP(6, 8, 16)
#undef STEP
        asm volatile("" : "+r"(safeCount) :: "memory");
    }
    return safeCount;
}

// Complete fast MADD as the generated code runs it: nv/nbc/nacc loads, fmacCore, flag bits (3 more movemasks +
// table), flag ring push, writeVf with dest. No cycle bookkeeping (see t_instr_fmac for that).
NOINLINE static uint64_t t_fmac_fast(uint64_t steps)
{
    VuState &s = *g_st;
    uint64_t cyc = 0;
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP(A, B, D, DEST) { __m128 n; const __m128 r = fmacMadd(s, cyc, DEST, nv(s, A), nbc(s, B, 1), nacc(s), n, false); writeVf(s, D, r, n, DEST); ++cyc; }
        STEP(1, 2, 9, 0xF) STEP(3, 4, 10, 0xE) STEP(5, 6, 11, 0xF) STEP(7, 8, 12, 0x8) STEP(1, 3, 13, 0xF) STEP(2, 4, 14, 0x7) STEP(5, 7, 15, 0xF) STEP(6, 8, 16, 0xE)
#undef STEP
        asm volatile("" : "+r"(cyc) :: "memory");
    }
    return cyc + s.flagTail;
}

// The same MADD always through the double-precision exact path (fmacExact).
NOINLINE static uint64_t t_fmac_exact(uint64_t steps)
{
    VuState &s = *g_st;
    uint64_t cyc = 0;
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP(A, B, D, DEST) { __m128 n; const __m128 r = fmacMadd(s, cyc, DEST, nv(s, A), nbc(s, B, 1), nacc(s), n, true); writeVf(s, D, r, n, DEST); ++cyc; }
        STEP(1, 2, 9, 0xF) STEP(3, 4, 10, 0xE) STEP(5, 6, 11, 0xF) STEP(7, 8, 12, 0x8) STEP(1, 3, 13, 0xF) STEP(2, 4, 14, 0x7) STEP(5, 7, 15, 0xF) STEP(6, 8, 16, 0xE)
#undef STEP
        asm volatile("" : "+r"(cyc) :: "memory");
    }
    return cyc + s.flagTail;
}

// Only the double-precision arithmetic of fmacExact (conversions, mul/add, compares, movemask_pd), no flags ring.
NOINLINE static uint64_t t_pd_core(uint64_t steps)
{
    VuState &s = *g_st;
    uint32_t acc = 0;
    const __m128d maxD = _mm_set1_pd(static_cast<double>(FLT_MAX));
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP(A, B)                                                                                     \
    {                                                                                                  \
        const __m128 a = nv(s, A), b = nv(s, B);                                                       \
        const __m128d lo = _mm_mul_pd(_mm_cvtps_pd(a), _mm_cvtps_pd(b));                               \
        const __m128d hi = _mm_mul_pd(_mm_cvtps_pd(_mm_movehl_ps(a, a)), _mm_cvtps_pd(_mm_movehl_ps(b, b))); \
        acc += maskBits(_mm_cmpgt_pd(lo, maxD), _mm_cmpgt_pd(hi, maxD));                               \
    }
        STEP(1, 2) STEP(3, 4) STEP(5, 6) STEP(7, 8) STEP(1, 3) STEP(2, 4) STEP(5, 7) STEP(6, 8)
#undef STEP
        asm volatile("" : "+r"(acc) :: "memory");
    }
    return acc;
}

// One full FMAC instruction of the generated code: budget check, 4-lane ready stall check of both sources, the
// fast MADD with flags and writeVf, setReady4 on the destination, ++cyc. exactEvery: 0 = never exact, N = every Nth.
template <unsigned ExactEvery>
NOINLINE static uint64_t t_instr_fmac(uint64_t steps)
{
    VuState &s = *g_st;
    uint64_t cyc = 64;
    const uint64_t budget = g_budget; // a run-time value, as ctx.budget
    uint32_t k = 0;
#define STEP(A, B, D, DEST)                                                                                         \
    if (cyc >= budget) break;                                                                                       \
    {                                                                                                               \
        uint64_t r = cyc;                                                                                           \
        r = rmax(r, s.vfReady[A][0]); r = rmax(r, s.vfReady[A][1]); r = rmax(r, s.vfReady[A][2]); r = rmax(r, s.vfReady[A][3]); \
        r = rmax(r, s.vfReady[B][1]);                                                                               \
        if (r > cyc) { if (r >= budget) { cyc = budget; break; } cyc = r; }                                         \
    }                                                                                                               \
    {                                                                                                               \
        const bool fx = ExactEvery != 0 && g_force[k++ & 255u];                                                     \
        __m128 n;                                                                                                   \
        const __m128 res = fmacMadd(s, cyc, DEST, nv(s, A), nbc(s, B, 1), nacc(s), n, fx);                          \
        writeVf(s, D, res, n, DEST);                                                                                \
        setReady4(s.vfReady[D], cyc + kFmacLatency);                                                                \
    }                                                                                                               \
    ++cyc;
    for (uint64_t i = 0; i < steps; i += 8)
    {
        STEP(1, 2, 9, 0xF) STEP(3, 4, 10, 0xE) STEP(5, 6, 11, 0xF) STEP(7, 8, 12, 0x8)
        STEP(1, 3, 13, 0xF) STEP(2, 4, 14, 0x7) STEP(5, 7, 15, 0xF) STEP(6, 8, 16, 0xE)
    }
#undef STEP
    return cyc + s.flagTail;
}

// One integer (lower) instruction as in the generated code (IADDIU-like): budget check, rmax stall check, the VI write
// with its branch-backup bookkeeping and the ready write.
NOINLINE static uint64_t t_instr_vi(uint64_t steps)
{
    VuState &s = *g_st;
    uint64_t cyc = 64;
    const uint64_t budget = g_budget; // a run-time value, as ctx.budget
#define STEP(SRC, DST)                                                                          \
    if (cyc >= budget) break;                                                                   \
    {                                                                                           \
        uint64_t r = cyc; r = rmax(r, s.viReady[SRC]);                                          \
        if (r > cyc) { if (r >= budget) { cyc = budget; break; } cyc = r; }                     \
    }                                                                                           \
    {                                                                                           \
        const int32_t oldVi = s.vi[DST];                                                        \
        s.vi[DST] = static_cast<int16_t>(s.vi[SRC] + 1);                                        \
        s.viBackupValid = false;                                                                \
        s.viReady[DST] = cyc + 1u;                                                              \
        s.viBackupValue = oldVi; s.viBackupReg = DST; s.viBackupValid = true;                   \
    }                                                                                           \
    ++cyc;
    for (uint64_t i = 0; i < steps; i += 8)
    {
        STEP(1, 2) STEP(3, 4) STEP(5, 6) STEP(7, 8) STEP(9, 10) STEP(11, 12) STEP(13, 14) STEP(2, 3)
    }
#undef STEP
    return cyc + static_cast<uint64_t>(s.vi[4]);
}

// _mm_getcsr + _mm_setcsr(round toward zero) + restore: what every VU1 run does on entry and exit when the
// rounding mode is not already toward zero (on ARM: FPCR reads and writes).
NOINLINE static uint64_t t_csr(uint64_t steps)
{
    uint64_t x = 0;
    for (uint64_t i = 0; i < steps; i += 8)
    {
#define STEP { const unsigned prev = _mm_getcsr(); _mm_setcsr(prev | 0x6000u); _mm_setcsr((_mm_getcsr() & ~0x6000u) | (prev & 0x6000u)); x += prev; }
        STEP STEP STEP STEP STEP STEP STEP STEP
#undef STEP
        asm volatile("" : "+r"(x));
    }
    return x;
}

// ---------------------------------------------------------------------------------------------------------------
// Harness.

struct PerfCounter
{
    int fd = -1;
    bool open(uint32_t type, uint64_t config)
    {
        perf_event_attr attr{};
        attr.type = type;
        attr.size = sizeof(attr);
        attr.config = config;
        attr.disabled = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        fd = static_cast<int>(syscall(SYS_perf_event_open, &attr, 0, -1, -1, 0));
        return fd >= 0;
    }
    void start() { if (fd >= 0) { ioctl(fd, PERF_EVENT_IOC_RESET, 0); ioctl(fd, PERF_EVENT_IOC_ENABLE, 0); } }
    uint64_t stop()
    {
        if (fd < 0) return 0;
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        uint64_t v = 0;
        if (read(fd, &v, sizeof(v)) != static_cast<ssize_t>(sizeof(v))) return 0;
        return v;
    }
};

static double nowNs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) * 1e9 + static_cast<double>(ts.tv_nsec);
}

static std::string readFile(const char *path)
{
    FILE *f = std::fopen(path, "r");
    if (!f) return "";
    char buf[512];
    std::string out;
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
    return out;
}

static double cpuMhz(int cpu)
{
    char path[128];
    std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
    const std::string s = readFile(path);
    return s.empty() ? 0.0 : std::atof(s.c_str()) / 1000.0;
}

struct Test
{
    const char *name;
    const char *what;
    uint64_t (*fn)(uint64_t);
};

static void resetState(VuState &s)
{
    std::memset(&s, 0, sizeof(s));
    for (int r = 0; r < 32; ++r)
        for (int c = 0; c < 4; ++c)
        {
            s.vf[r][c] = 0.5f + 0.01f * static_cast<float>(r * 4 + c) * ((r + c) & 1 ? -1.0f : 1.0f);
            s.norm[r][c] = s.vf[r][c];
        }
    for (int c = 0; c < 4; ++c) { s.acc[c] = 0.25f; s.norm[32][c] = 0.25f; }
    for (int r = 0; r < 16; ++r) s.vi[r] = r * 3;
    for (int i = 0; i < 16384; ++i) s.mem[i] = static_cast<uint8_t>(i * 7);
    // Ready times mostly in the past (no stall), as in steady code; a few in the future set the stall branch now and then.
    for (int r = 0; r < 32; ++r) setReady4(s.vfReady[r], 0);
    for (int r = 0; r < 16; ++r) s.viReady[r] = 0;
}

int main(int argc, char **argv)
{
    int cpu = -1, reps = 7;
    double targetMs = 250.0;
    std::string only, csvPath;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--cpu" && i + 1 < argc) cpu = std::atoi(argv[++i]);
        else if (a == "--reps" && i + 1 < argc) reps = std::max(3, std::atoi(argv[++i]));
        else if (a == "--ms" && i + 1 < argc) targetMs = std::atof(argv[++i]);
        else if (a == "--only" && i + 1 < argc) only = argv[++i];
        else if (a == "--csv" && i + 1 < argc) csvPath = argv[++i];
        else { std::fprintf(stderr, "usage: %s [--cpu N] [--reps 7] [--ms 250] [--only name] [--csv file]\n", argv[0]); return 2; }
    }
    if (cpu >= 0)
    {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu, &set);
        if (sched_setaffinity(0, sizeof(set), &set) != 0) std::fprintf(stderr, "[mb] affinity to cpu %d failed: %s\n", cpu, std::strerror(errno));
    }
    const int runCpu = sched_getcpu();

    static VuState state;
    g_st = &state;
    for (int i = 0; i < 256; ++i) g_force[i] = 0;

    PerfCounter cycles, instrs;
    const bool havePerf = cycles.open(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES) && instrs.open(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

    char gov[160];
    std::snprintf(gov, sizeof(gov), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", runCpu);
    std::printf("[mb-env] arch=%s compiler=%s cpu=%d mhz=%.0f governor=%s perf=%s reps=%d target=%.0fms model=%s\n", MB_ARCH,
                __VERSION__, runCpu, cpuMhz(runCpu), readFile(gov).c_str(), havePerf ? "yes" : "no (cycles estimated from mhz)", reps,
                targetMs, readFile("/proc/device-tree/model").c_str());

    const Test tests[] = {
        {"loop", "empty loop step (harness floor)", t_loop},
        {"budget", "budget check + ++cyc", t_budget},
        {"rmax2", "budget + 2 rmax (VI ready) + ready write", t_rmax2},
        {"rmax_vf", "budget + 8 rmax (VF 4-lane ready) + setReady4", t_rmaxvf},
        {"instr_vi", "integer lower instruction (budget, rmax, VI write, branch backup)", t_instr_vi},
        {"vnorm_thr", "vnorm, independent, memory to memory", t_vnorm_thr},
        {"vnorm_lat", "vnorm, dependent chain", t_vnorm_lat},
        {"writevf", "writeVf with dest blend (raw + norm stores)", t_writevf},
        {"ldst", "LQ/SQ: qaddr + ldq + storeDest", t_ldst},
        {"movemask_thr", "movemask_ps + kRev4, independent", t_movemask_thr},
        {"movemask_lat", "movemask_ps -> table -> vector, dependent", t_movemask_lat},
        {"muladd_thr", "mul_ps + add_ps, 8 independent chains", t_muladd_thr},
        {"muladd_lat", "mul_ps + add_ps, dependent chain", t_muladd_lat},
        {"fmaccore", "fmacCore<Madd>: math + safety checks + 1 movemask", t_fmaccore},
        {"fmac_fast", "MADD fast path + flags + ring push + writeVf", t_fmac_fast},
        {"fmac_exact", "MADD exact (double) path + flags + ring push + writeVf", t_fmac_exact},
        {"pd_core", "double math only: 4 cvtps_pd + 2 mul_pd + 2 cmp + movemask_pd", t_pd_core},
        {"instr_fmac", "full FMAC instruction: budget + VF ready stall + fast MADD + writeVf + setReady4", t_instr_fmac<0>},
        {"instr_fmac_x10", "the same, 1 in 10 through the exact path", t_instr_fmac<10>},
        {"instr_fmac_x2", "the same, 1 in 2 through the exact path", t_instr_fmac<2>},
        {"csr", "getcsr + setcsr(RZ) + getcsr + setcsr(restore)", t_csr},
    };

    FILE *csv = csvPath.empty() ? nullptr : std::fopen(csvPath.c_str(), "w");
    if (csv)
        std::fprintf(csv, "test,ops,ns_per_op_median,ns_per_op_min,ns_per_op_max,cycles_per_op,cycles_source,instr_per_op,ipc,mhz\n");

    for (const Test &t : tests)
    {
        if (!only.empty() && only != t.name)
            continue;
        if (std::strcmp(t.name, "instr_fmac_x10") == 0)
            for (int i = 0; i < 256; ++i) g_force[i] = (i % 10) == 0;
        else if (std::strcmp(t.name, "instr_fmac_x2") == 0)
            for (int i = 0; i < 256; ++i) g_force[i] = (i % 2) == 0;
        else
            for (int i = 0; i < 256; ++i) g_force[i] = 0;

        // Calibrate the step count to ~targetMs per repetition.
        uint64_t steps = 1u << 16;
        for (;;)
        {
            resetState(state);
            const double t0 = nowNs();
            g_sink = g_sink + t.fn(steps);
            const double dt = nowNs() - t0;
            if (dt > targetMs * 1e6 * 0.2 || steps > (1ull << 40)) { steps = static_cast<uint64_t>(static_cast<double>(steps) * targetMs * 1e6 / std::max(dt, 1.0)); break; }
            steps *= 4;
        }
        steps = std::max<uint64_t>(8, steps & ~7ull);

        std::vector<double> ns;
        std::vector<double> cyc, ins;
        double mhzSum = 0.0;
        for (int r = 0; r < reps; ++r)
        {
            resetState(state);
            cycles.start();
            instrs.start();
            const double t0 = nowNs();
            g_sink = g_sink + t.fn(steps);
            const double dt = nowNs() - t0;
            const uint64_t c = cycles.stop(), n = instrs.stop();
            ns.push_back(dt / static_cast<double>(steps));
            cyc.push_back(static_cast<double>(c) / static_cast<double>(steps));
            ins.push_back(static_cast<double>(n) / static_cast<double>(steps));
            mhzSum += cpuMhz(sched_getcpu());
        }
        std::vector<double> sorted = ns;
        std::sort(sorted.begin(), sorted.end());
        const double med = sorted[sorted.size() / 2], mn = sorted.front(), mx = sorted.back();
        std::sort(cyc.begin(), cyc.end());
        std::sort(ins.begin(), ins.end());
        const double mhz = mhzSum / reps;
        const double cycPerOp = havePerf ? cyc[cyc.size() / 2] : med * mhz / 1000.0;
        const double insPerOp = havePerf ? ins[ins.size() / 2] : 0.0;
        const double ipc = havePerf && cycPerOp > 0.0 ? insPerOp / cycPerOp : 0.0;
        std::printf("[mb] %-15s ns/op=%8.3f (min %.3f max %.3f, spread %.1f%%) cycles/op=%7.2f%s instr/op=%6.2f ipc=%.2f ops=%" PRIu64 " | %s\n",
                    t.name, med, mn, mx, med > 0.0 ? 100.0 * (mx - mn) / med : 0.0, cycPerOp, havePerf ? "" : " (est)", insPerOp, ipc, steps, t.what);
        std::fflush(stdout);
        if (csv)
            std::fprintf(csv, "%s,%" PRIu64 ",%.4f,%.4f,%.4f,%.3f,%s,%.3f,%.3f,%.0f\n", t.name, steps, med, mn, mx, cycPerOp,
                         havePerf ? "perf" : "estimated", insPerOp, ipc, mhz);
    }
    if (csv)
        std::fclose(csv);
    std::printf("[mb-end] sink=%" PRIu64 "\n", static_cast<uint64_t>(g_sink));
    return 0;
}
