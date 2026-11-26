// RK3326 port: x86 SSE header on ARM64 maps to sse2neon (NEON).
#pragma once
#if defined(__aarch64__) || defined(__arm__)
#include "sse2neon.h"
#else
#include_next <x86intrin.h>
#endif
