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
#include <ripple.h>
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

#if __has_Float16__
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
#if defined(__hexagon__) && defined(__HVX__)
inline _Float16 rsqrtf16_fn(_Float16 x) { return rsqrtf16(x); }
inline _Float16 hypotf16_fn(_Float16 x, _Float16 y) { return hypotf16(x, y); }
#else
inline _Float16 rsqrtf16_fn(_Float16 x) {
  return static_cast<_Float16>(rsqrtf_fn(static_cast<float>(x)));
}
inline _Float16 hypotf16_fn(_Float16 x, _Float16 y) {
  return static_cast<_Float16>(
      hypotf_fn(static_cast<float>(x), static_cast<float>(y)));
}
#endif
inline _Float16 truncf16_fn(_Float16 x) { return truncf16(x); }
inline _Float16 roundf16_fn(_Float16 x) { return roundf16(x); }
inline _Float16 floorf16_fn(_Float16 x) { return floorf16(x); }
inline _Float16 ceilf16_fn(_Float16 x) { return ceilf16(x); }
#endif

#if __has_bf16__
inline __bf16 sinbf16_fn(__bf16 x) { return sinbf16(x); }
inline __bf16 cosbf16_fn(__bf16 x) { return cosbf16(x); }
inline __bf16 tanbf16_fn(__bf16 x) { return tanbf16(x); }
inline __bf16 asinbf16_fn(__bf16 x) { return asinbf16(x); }
inline __bf16 acosbf16_fn(__bf16 x) { return acosbf16(x); }
inline __bf16 atanbf16_fn(__bf16 x) { return atanbf16(x); }
inline __bf16 sinhbf16_fn(__bf16 x) { return sinhbf16(x); }
inline __bf16 coshbf16_fn(__bf16 x) { return coshbf16(x); }
inline __bf16 tanhbf16_fn(__bf16 x) { return tanhbf16(x); }
inline __bf16 expbf16_fn(__bf16 x) { return expbf16(x); }
inline __bf16 logbf16_fn(__bf16 x) { return logbf16(x); }
inline __bf16 powbf16_fn(__bf16 x, __bf16 y) { return powbf16(x, y); }
inline __bf16 sqrtbf16_fn(__bf16 x) { return sqrtbf16(x); }
// The public bf16 API does not provide rsqrt/hypot on every target.
// Use float Ripple operations and round their results to bf16.
inline __bf16 rsqrtbf16_fn(__bf16 x) {
  return static_cast<__bf16>(rsqrtf_fn(static_cast<float>(x)));
}
inline __bf16 hypotbf16_fn(__bf16 x, __bf16 y) {
  return static_cast<__bf16>(
      hypotf_fn(static_cast<float>(x), static_cast<float>(y)));
}
inline __bf16 truncbf16_fn(__bf16 x) { return truncbf16(x); }
inline __bf16 roundbf16_fn(__bf16 x) { return roundbf16(x); }
inline __bf16 floorbf16_fn(__bf16 x) { return floorbf16(x); }
inline __bf16 ceilbf16_fn(__bf16 x) { return ceilbf16(x); }
#endif
