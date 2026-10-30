//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#if defined(__FLT16_MANT_DIG__)

#include <cmath>
#include <cstring>
#include <ripple.h>
#include <stddef.h>
#include <stdint.h>

namespace {

// Ripple implementation under test.
inline __attribute((always_inline)) _Float16 rsqrt_impl(_Float16 xin) {
  constexpr _Float16 half = 0.5f;
  constexpr _Float16 two_power_10 = 1024.f;
  constexpr _Float16 two_power_5 = 32.f;
  constexpr _Float16 smallf16 = 0.000061f;

  //  {{{ quake-III magic for good initial guess.

  uint16_t xin_int = *(uint16_t *)&xin;
  uint16_t magic_step_int = 0x5994 - (xin_int >> 1);
  _Float16 y0 = *(_Float16 *)&magic_step_int;

  // }}}

  bool xin_subnormal = xin <= smallf16; // subnormal_detector;
  const _Float16 scale = xin_subnormal ? 1024.f : 1;
  const _Float16 rscale = xin_subnormal ? 32.f : 1;

  float half_x = xin * (half * scale); // use 32 bits for stable Newton update.

  float y1 = y0 * (1.5f - half_x * (y0 * y0)); // First iter.
  float y2 = y1 * (1.5f - half_x * (y1 * y1)); // Second iter.
  return y2 * rscale;
}

void fp16_rsqrt_ripple(size_t N,
                       const _Float16 *in __attribute__((aligned(128))),
                       _Float16 *out __attribute__((aligned(128)))) {
  auto BS = ripple_set_block_shape(0, 64);
  auto v0 = ripple_id(BS, 0);
  for (size_t j = 0; j + 63 <= N; j += 64) {
    const _Float16 *inptr = ripple_ptr_alignment(&in[j], 128);
    _Float16 *outptr = ripple_ptr_alignment(&out[j], 128);

    outptr[v0] = rsqrt_impl(inptr[v0]);
  }
}

// Locally authored scalar oracle. The calculation is IEEE binary32 and the
// final binary16 conversion is explicitly round-to-nearest, ties-to-even.
_Float16 fp16_from_float_rne(float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000);
  const uint32_t exponent = (bits >> 23) & 0xff;
  const uint32_t fraction = bits & 0x7fffff;
  uint16_t result;
  if (exponent == 0xff) {
    result = fraction == 0 ? 0x7c00 : 0x7e00;
  } else {
    int half_exponent = static_cast<int>(exponent) - 127 + 15;
    if (half_exponent >= 31) {
      result = 0x7c00;
    } else if (half_exponent <= 0) {
      if (half_exponent < -10) {
        result = 0;
      } else {
        const uint32_t significand = fraction | 0x800000;
        const unsigned shift = static_cast<unsigned>(14 - half_exponent);
        uint32_t rounded = significand >> shift;
        const uint32_t remainder = significand & ((uint32_t(1) << shift) - 1);
        const uint32_t halfway = uint32_t(1) << (shift - 1);
        if (remainder > halfway || (remainder == halfway && (rounded & 1)))
          ++rounded;
        result = static_cast<uint16_t>(rounded);
      }
    } else {
      uint32_t rounded = fraction >> 13;
      const uint32_t remainder = fraction & 0x1fff;
      if (remainder > 0x1000 || (remainder == 0x1000 && (rounded & 1)))
        ++rounded;
      if (rounded == 0x400) {
        rounded = 0;
        ++half_exponent;
      }
      result = half_exponent >= 31
                   ? 0x7c00
                   : static_cast<uint16_t>((half_exponent << 10) | rounded);
    }
  }
  result |= sign;
  _Float16 out;
  std::memcpy(&out, &result, sizeof(out));
  return out;
}

void fp16_rsqrt_reference(size_t N, const _Float16 *in, _Float16 *out) {
  for (size_t i = 0; i < N; ++i)
    out[i] = fp16_from_float_rne(1.0f / std::sqrt(static_cast<float>(in[i])));
}

alignas(128) _Float16 X[17280];
alignas(128) _Float16 YRef[17280];
alignas(128) _Float16 Y[17280];

} // namespace

#include <ripple-test-suite/ripple-test-suite.h>
using namespace ripple_test_suite;
enum KernelVariant { Reference, Ripple };

template <KernelVariant KT> class RsqrtTest : public Test {
  static constexpr int N = 17280;
  static constexpr double RelativeTolerance = 0.01;
  static constexpr double AbsoluteTolerance = 0.00006103515625;

public:
  RsqrtTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; i++) {
      // Keep the approximation's domain finite, normal, and strictly positive.
      X[i] = static_cast<_Float16>(0.001f + 1729.0f * randn());
      Y[i] = YRef[i] = 0;
    }
    fp16_rsqrt_reference(N, X, YRef);
  }

  void run(unsigned) override {
    if (KT == Reference)
      fp16_rsqrt_reference(N, X, Y);
    if (KT == Ripple)
      fp16_rsqrt_ripple(N, X, Y);
  }

  bool verify() const override {
    return isclose(N, Y, YRef, RelativeTolerance, AbsoluteTolerance);
  }
};

DefineTest<RsqrtTest<Reference>> RsqrtRefFp16("Rsqrt<ref,f16>");
DefineTest<RsqrtTest<Ripple>> RsqrtRippleFp16("Rsqrt<ripple,f16>");
#endif // __FLT16_MANT_DIG__
