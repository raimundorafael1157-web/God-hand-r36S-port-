// asm_probe: one function per SSE intrinsic used by the VU1 code, compiled only to assembly (-S), for both targets,
// to compare how many and which instructions each one becomes (x86 SSE natively, ARM through sse2neon v1.8.0).
// Not run; read by count_asm.py.
#if defined(__aarch64__)
#include "sse2neon.h"
#else
#include <immintrin.h>
#endif
#include <cstdint>

#define P extern "C" __attribute__((noinline))

P __m128 p_add_ps(__m128 a, __m128 b) { return _mm_add_ps(a, b); }
P __m128 p_mul_ps(__m128 a, __m128 b) { return _mm_mul_ps(a, b); }
P __m128 p_and_ps(__m128 a, __m128 b) { return _mm_and_ps(a, b); }
P __m128 p_andnot_ps(__m128 a, __m128 b) { return _mm_andnot_ps(a, b); }
P __m128 p_or_ps(__m128 a, __m128 b) { return _mm_or_ps(a, b); }
P __m128 p_cmpeq_ps(__m128 a, __m128 b) { return _mm_cmpeq_ps(a, b); }
P __m128 p_cmpge_ps(__m128 a, __m128 b) { return _mm_cmpge_ps(a, b); }
P __m128 p_cmplt_ps(__m128 a, __m128 b) { return _mm_cmplt_ps(a, b); }
P __m128 p_min_ps(__m128 a, __m128 b) { return _mm_min_ps(a, b); }
P __m128 p_max_ps(__m128 a, __m128 b) { return _mm_max_ps(a, b); }
P int p_movemask_ps(__m128 a) { return _mm_movemask_ps(a); }
P int p_movemask_pd(__m128d a) { return _mm_movemask_pd(a); }
P int p_movemask_epi8(__m128i a) { return _mm_movemask_epi8(a); }
P __m128 p_shuffle_ps_bc(__m128 a) { return _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1)); }
P __m128 p_shuffle_ps_mix(__m128 a, __m128 b) { return _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1)); }
P __m128 p_shuffle_ps_cross(__m128 a) { return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)); }
P __m128 p_movehl_ps(__m128 a) { return _mm_movehl_ps(a, a); }
P __m128d p_cvtps_pd(__m128 a) { return _mm_cvtps_pd(a); }
P __m128d p_add_pd(__m128d a, __m128d b) { return _mm_add_pd(a, b); }
P __m128d p_mul_pd(__m128d a, __m128d b) { return _mm_mul_pd(a, b); }
P __m128d p_cmpgt_pd(__m128d a, __m128d b) { return _mm_cmpgt_pd(a, b); }
P __m128d p_cmplt_pd(__m128d a, __m128d b) { return _mm_cmplt_pd(a, b); }
P __m128d p_cmpeq_pd(__m128d a, __m128d b) { return _mm_cmpeq_pd(a, b); }
P __m128i p_cmpeq_epi32(__m128i a, __m128i b) { return _mm_cmpeq_epi32(a, b); }
P float p_cvtss_f32(__m128 a) { return _mm_cvtss_f32(a); }
P __m128 p_set1_ps_mem(const float *p) { return _mm_set1_ps(*p); }
P __m128 p_loadu_ps(const float *p) { return _mm_loadu_ps(p); }
P void p_storeu_ps(float *p, __m128 a) { _mm_storeu_ps(p, a); }
P float p_extract_ps(__m128 a) { int v = _mm_extract_ps(a, 2); float f; __builtin_memcpy(&f, &v, 4); return f; }
P unsigned p_getcsr() { return _mm_getcsr(); }
P void p_setcsr(unsigned v) { _mm_setcsr(v); }
P uint64_t p_rmax(uint64_t a, uint64_t b) { return a < b ? b : a; }
