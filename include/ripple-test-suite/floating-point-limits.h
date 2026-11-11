//===----------------------------------------------------------------------===//
//
// (c) 2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#pragma once

#include <limits>
#include <ripple.h>

namespace ripple_test_suite {

// Prefer the standard library's definition whenever it is available.
template <typename T, bool Specialized = std::numeric_limits<T>::is_specialized>
struct FloatLimits : std::numeric_limits<T> {};

// The SDK's <limits> leaves FP16/BF16 unspecialized and returns zero values.
// Supply fallbacks only for libraries that lack these specializations.
#if __has_Float16__
template <> struct FloatLimits<_Float16, false> {
  static constexpr bool has_infinity = true;
  // Binary16: max = (2 - 2^-10) * 2^15 = 65504; min normal = 2^-14.
  // Reuse Clang's target-defined limits rather than spelling those values out.
  static constexpr _Float16 max() { return __FLT16_MAX__; }
  static constexpr _Float16 min() { return __FLT16_MIN__; }
  static constexpr _Float16 infinity() {
    return __builtin_inff();
  }
};
#endif

#if __has_bf16__
template <> struct FloatLimits<__bf16, false> {
  static constexpr bool has_infinity = true;
  // Layout source: llvm-project/libc/src/__support/FPUtil/FPBits.h,
  // FPLayout<FPType::BFloat16>.
  // 8 exponent bits (bias 127), 7 fraction bits give
  // max = (2 - 2^-7) * 2^127 = 0x1.fep127; min normal = 2^-126, as in FP32.
  static constexpr __bf16 max() {
#ifdef __BFLT16_MAX__
    return __BFLT16_MAX__;
#else
    return 0x1.fep127f;
#endif
  }
  static constexpr __bf16 min() {
#ifdef __BFLT16_MIN__
    return __BFLT16_MIN__;
#else
    return __FLT_MIN__;
#endif
  }
  static constexpr __bf16 infinity() {
    return __builtin_inff();
  }
};
#endif

} // namespace ripple_test_suite
