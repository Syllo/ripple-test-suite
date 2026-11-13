//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#pragma once

// Keep this helper independent of the test framework and standard-library
// math headers: SDK inline math wrappers can bypass Ripple's vector candidates.
#if defined(__x86_64__)
#include <immintrin.h>
#elif defined(__HVX__)
#include <hexagon_types.h>
#elif defined(__aarch64__)
#include <arm_neon.h>
#endif

#if defined(__x86_64__)
// Match the widest fixed-size vector enabled by the compiler's target flags.
#if defined(__AVX512F__)
#define RIPPLE_BLOCK_SIZE(T) (sizeof(__m512) / sizeof(T))
#elif defined(__AVX__)
#define RIPPLE_BLOCK_SIZE(T) (sizeof(__m256) / sizeof(T))
#else
#define RIPPLE_BLOCK_SIZE(T) (sizeof(__m128) / sizeof(T))
#endif
#elif defined(__HVX__)
#define RIPPLE_BLOCK_SIZE(T) (sizeof(HVX_Vector) / sizeof(T))
#elif defined(__aarch64__)
// SVE types are sizeless; use NEON for a compile-time block size.
#define RIPPLE_BLOCK_SIZE(T) (sizeof(uint8x16_t) / sizeof(T))
#endif
