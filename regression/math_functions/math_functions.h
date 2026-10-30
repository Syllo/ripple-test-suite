//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdlib>
#include <ripple_math.h>

template <typename T, T (*OP)(T)>
void Ripple_unary_math(size_t N, T *output, const T *input);

template <typename T, T (*OP)(T, T)>
void Ripple_binary_math(size_t N, T *output, const T *input1, const T *input2);

inline float sinf_fn(float x) { return sinf(x); }

inline float cosf_fn(float x) { return cosf(x); }

inline float tanf_fn(float x) { return tanf(x); }

inline float asinf_fn(float x) { return asinf(x); }

inline float acosf_fn(float x) { return acosf(x); }

inline float atanf_fn(float x) { return atanf(x); }

inline float sinhf_fn(float x) { return sinhf(x); }

inline float coshf_fn(float x) { return coshf(x); }

inline float tanhf_fn(float x) { return tanhf(x); }

inline float expf_fn(float x) { return expf(x); }

inline float logf_fn(float x) { return logf(x); }

inline float powf_fn(float x, float y) { return powf(x, y); }

inline float sqrtf_fn(float x) { return sqrtf(x); }

#if defined(__hexagon__) && defined(__HVX__)
inline float rsqrtf_fn(float x) { return rsqrtf(x); }
#else
inline float rsqrtf_fn(float x) { return 1.0f / sqrtf(x); }
#endif

#if defined(__hexagon__) && defined(__HVX__)
inline float hypotf_fn(float x, float y) { return hypotf(x, y); }
#else
inline float hypotf_fn(float x, float y) { return __builtin_hypotf(x, y); }
#endif

inline float truncf_fn(float x) { return truncf(x); }

inline float roundf_fn(float x) { return roundf(x); }

inline float floorf_fn(float x) { return floorf(x); }

inline float ceilf_fn(float x) { return ceilf(x); }

// FIXME: Re-enable these wrappers with the FP16 tests when the Hexagon runtime
// provides the scalar FP16 math entry points needed for references and tails.
#if 0
inline _Float16 sinf16_fn(_Float16 x) { return sinf16(x); }
inline _Float16 cosf16_fn(_Float16 x) { return cosf16(x); }
inline _Float16 tanf16_fn(_Float16 x) { return tanf16(x); }
inline _Float16 asinf16_fn(_Float16 x) { return asinf16(x); }
inline _Float16 acosf16_fn(_Float16 x) { return acosf16(x); }
inline _Float16 atanf16_fn(_Float16 x) { return atanf16(x); }
inline _Float16 sinhf16_fn(_Float16 x) { return sinhf16(x); }
inline _Float16 coshf16_fn(_Float16 x) { return coshf16(x); }
inline _Float16 tanhf16_fn(_Float16 x) { return tanhf16(x); }
inline _Float16 expf16_fn(_Float16 x) { return expf16(x); }
inline _Float16 logf16_fn(_Float16 x) { return logf16(x); }
inline _Float16 powf16_fn(_Float16 x, _Float16 y) { return powf16(x, y); }
inline _Float16 sqrtf16_fn(_Float16 x) { return sqrtf16(x); }
inline _Float16 rsqrtf16_fn(_Float16 x) { return rsqrtf16(x); }
inline _Float16 hypotf16_fn(_Float16 x, _Float16 y) { return hypotf16(x, y); }
inline _Float16 truncf16_fn(_Float16 x) { return truncf16(x); }
inline _Float16 roundf16_fn(_Float16 x) { return roundf16(x); }
inline _Float16 floorf16_fn(_Float16 x) { return floorf16(x); }
inline _Float16 ceilf16_fn(_Float16 x) { return ceilf16(x); }
#endif
