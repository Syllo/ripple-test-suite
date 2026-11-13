//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple-test-suite/ripple-test-suite.h>

#include "math_functions.h"
#include <ripple.h>
#include <stdio.h>

// Unary math function
template <typename T, T (*OP)(T)>
void Ripple_unary_math(size_t N, T *output, const T *input) {
  auto BlockShape = ripple_set_block_shape(0, RIPPLE_BLOCK_SIZE(T));
  ripple_parallel(BlockShape, 0);
  for (size_t i = 0; i < N; i++)
    output[i] = OP(input[i]);
}

// Binary math function
template <typename T, T (*OP)(T, T)>
void Ripple_binary_math(size_t N, T *output, const T *input1, const T *input2) {
  auto BlockShape = ripple_set_block_shape(0, RIPPLE_BLOCK_SIZE(T));
  ripple_parallel(BlockShape, 0);
  for (size_t i = 0; i < N; i++)
    output[i] = OP(input1[i], input2[i]);
}

// float unary
template void Ripple_unary_math<float, sinf_fn>(size_t, float *, const float *);
template void Ripple_unary_math<float, cosf_fn>(size_t, float *, const float *);
template void Ripple_unary_math<float, tanf_fn>(size_t, float *, const float *);
template void Ripple_unary_math<float, asinf_fn>(size_t, float *,
                                                 const float *);
template void Ripple_unary_math<float, acosf_fn>(size_t, float *,
                                                 const float *);
template void Ripple_unary_math<float, atanf_fn>(size_t, float *,
                                                 const float *);
template void Ripple_unary_math<float, sinhf_fn>(size_t, float *,
                                                 const float *);
template void Ripple_unary_math<float, coshf_fn>(size_t, float *,
                                                 const float *);
template void Ripple_unary_math<float, tanhf_fn>(size_t, float *,
                                                 const float *);
template void Ripple_unary_math<float, expf_fn>(size_t, float *, const float *);
template void Ripple_unary_math<float, logf_fn>(size_t, float *, const float *);
template void Ripple_unary_math<float, sqrtf_fn>(size_t, float *,
                                                 const float *);
template void Ripple_unary_math<float, rsqrtf_fn>(size_t, float *,
                                                  const float *);
template void Ripple_unary_math<float, truncf_fn>(size_t, float *,
                                                  const float *);
template void Ripple_unary_math<float, roundf_fn>(size_t, float *,
                                                  const float *);
template void Ripple_unary_math<float, floorf_fn>(size_t, float *,
                                                  const float *);
template void Ripple_unary_math<float, ceilf_fn>(size_t, float *,
                                                 const float *);

#if __has_Float16__
template void Ripple_unary_math<_Float16, sinf16_fn>(size_t, _Float16 *,
                                                     const _Float16 *);
template void Ripple_unary_math<_Float16, cosf16_fn>(size_t, _Float16 *,
                                                     const _Float16 *);
template void Ripple_unary_math<_Float16, tanf16_fn>(size_t, _Float16 *,
                                                     const _Float16 *);
template void Ripple_unary_math<_Float16, asinf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *);
template void Ripple_unary_math<_Float16, acosf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *);
template void Ripple_unary_math<_Float16, atanf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *);
template void Ripple_unary_math<_Float16, sinhf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *);
template void Ripple_unary_math<_Float16, coshf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *);
template void Ripple_unary_math<_Float16, tanhf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *);
template void Ripple_unary_math<_Float16, expf16_fn>(size_t, _Float16 *,
                                                     const _Float16 *);
template void Ripple_unary_math<_Float16, logf16_fn>(size_t, _Float16 *,
                                                     const _Float16 *);
template void Ripple_unary_math<_Float16, sqrtf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *);
template void Ripple_unary_math<_Float16, rsqrtf16_fn>(size_t, _Float16 *,
                                                       const _Float16 *);
template void Ripple_unary_math<_Float16, truncf16_fn>(size_t, _Float16 *,
                                                       const _Float16 *);
template void Ripple_unary_math<_Float16, roundf16_fn>(size_t, _Float16 *,
                                                       const _Float16 *);
template void Ripple_unary_math<_Float16, floorf16_fn>(size_t, _Float16 *,
                                                       const _Float16 *);
template void Ripple_unary_math<_Float16, ceilf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *);
#endif

// float binary
template void Ripple_binary_math<float, powf_fn>(size_t, float *, const float *,
                                                 const float *);
template void Ripple_binary_math<float, hypotf_fn>(size_t, float *,
                                                   const float *,
                                                   const float *);

#if __has_Float16__
template void Ripple_binary_math<_Float16, powf16_fn>(size_t, _Float16 *,
                                                      const _Float16 *,
                                                      const _Float16 *);
template void Ripple_binary_math<_Float16, hypotf16_fn>(size_t, _Float16 *,
                                                        const _Float16 *,
                                                        const _Float16 *);
#endif

#if __has_bf16__
template void Ripple_unary_math<__bf16, sinbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, cosbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, tanbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, asinbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, acosbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, atanbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, sinhbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, coshbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, tanhbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, expbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, logbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, sqrtbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, rsqrtbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, truncbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, roundbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, floorbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_unary_math<__bf16, ceilbf16_fn>(
    size_t, __bf16 *, const __bf16 *);
template void Ripple_binary_math<__bf16, powbf16_fn>(
    size_t, __bf16 *, const __bf16 *, const __bf16 *);
template void Ripple_binary_math<__bf16, hypotbf16_fn>(
    size_t, __bf16 *, const __bf16 *, const __bf16 *);
#endif
