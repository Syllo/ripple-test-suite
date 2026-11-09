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

#include <limits>
#include <vector>

namespace {
using namespace ripple_test_suite;

// Reference implementations
template <typename T> T hypot_ref(T x, T y) {
  return static_cast<T>(sqrt(x * x + y * y));
}
template <typename T> T rsqrt_ref(T x) { return static_cast<T>(1.0) / sqrt(x); }

template <typename T>
void fill_linear(std::vector<T> &Input, double Min, double Max) {
  const double Step = (Max - Min) / (Input.size() - 1);
  for (size_t i = 0; i < Input.size(); ++i)
    Input[i] = static_cast<T>(Min + Step * i);
}

template <typename T, float (*OP)(float)>
void prepare_unary_input(std::vector<T> &Input) {
  if (OP == asinf_fn || OP == acosf_fn || OP == tanf_fn) {
    fill_linear(Input, -1.0, 1.0);
  } else if (OP == expf_fn || OP == sinhf_fn || OP == coshf_fn) {
    fill_linear(Input, -4.0, 4.0);
  } else if (OP == logf_fn || OP == rsqrt_ref<float>) {
    fill_linear(Input, 0.125, 16.0);
  } else if (OP == sqrtf_fn) {
    fill_linear(Input, 0.0, 16.0);
  } else {
    fill_linear(Input, -8.0, 8.0);
  }
}

template <typename T, float (*OP)(float, float)>
void prepare_binary_input(std::vector<T> &Input1, std::vector<T> &Input2) {
  if (OP == powf_fn) {
    fill_linear(Input1, 0.5, 2.0);
    fill_linear(Input2, 0.0, 1.0);
  } else {
    fill_linear(Input1, -8.0, 8.0);
    fill_linear(Input2, 4.0, -4.0);
  }
}

template <typename T, size_t NumValues>
void repeat_values(std::vector<T> &Input,
                   const float (&Values)[NumValues]) {
  for (size_t i = 0; i < Input.size(); ++i)
    Input[i] = static_cast<T>(Values[i % NumValues]);
}

template <typename T, float (*OP)(float)>
void prepare_unary_domain_input(std::vector<T> &Input) {
  if (OP == asinf_fn || OP == acosf_fn) {
    const float Values[] = {-4.0f, -1.25f, 1.25f, 4.0f};
    repeat_values(Input, Values);
  } else if (OP == logf_fn) {
    const float Values[] = {-4.0f, -1.0f, -0.25f};
    repeat_values(Input, Values);
  } else if (OP == sqrtf_fn) {
    const float Values[] = {-4.0f, -1.0f, -0.25f, -0.0f};
    repeat_values(Input, Values);
  } else if (OP == rsqrt_ref<float>) {
    const float Values[] = {-4.0f, -1.0f, -0.25f};
    repeat_values(Input, Values);
  }
}

template <typename T> void prepare_zero_input(std::vector<T> &Input) {
  const float Values[] = {-0.0f, 0.0f};
  repeat_values(Input, Values);
}

template <typename T>
void prepare_pow_domain_input(std::vector<T> &Input1,
                              std::vector<T> &Input2) {
  const float Bases[] = {-4.0f, -2.0f, -1.0f, -0.5f};
  const float Exponents[] = {0.5f, 1.5f, -0.5f, 2.5f};
  repeat_values(Input1, Bases);
  repeat_values(Input2, Exponents);
}

bool same_special_value(float Actual, float Expected) {
  if (isnan(Expected))
    return isnan(Actual);
  if (isinf(Expected))
    return isinf(Actual) &&
           __builtin_signbit(Actual) == __builtin_signbit(Expected);
  if (Expected == 0.0f)
    return Actual == 0.0f &&
           __builtin_signbit(Actual) == __builtin_signbit(Expected);
  return Actual == Expected;
}

// Widen the quantized input to float, use scalar libm, and round the result
// once to the destination type. This does not require scalar f16/bf16 libm.
template <typename T, float (*OP)(float)>
void ref_unary_math(size_t N, T *output, const T *input) {
  for (size_t i = 0; i < N; i++)
    output[i] = static_cast<T>(OP(static_cast<float>(input[i])));
}

template <typename T, float (*OP)(float, float)>
void ref_binary_math(size_t N, T *output, const T *input1, const T *input2) {
  for (size_t i = 0; i < N; i++)
    output[i] = static_cast<T>(
        OP(static_cast<float>(input1[i]), static_cast<float>(input2[i])));
}

template <typename T> class TestBase : public Test {
protected:
  const size_t N;
  std::vector<T> Input1, Input2, Output;
  std::vector<T> RefOutput;

  void reset_output(float Value = std::numeric_limits<float>::quiet_NaN()) {
    std::fill(Output.begin(), Output.end(), static_cast<T>(Value));
  }

public:
  TestBase(TestFramework &TF, size_t N)
      : Test(TF), N(N), Input1(N), Input2(N), Output(N), RefOutput(N) {}

  bool verify() const override {
    // Compare in double so bf16/fp16 subtraction cannot round away an error.
    const double RelativeTolerance = sizeof(T) == 2 ? 1e-2 : 1e-5;
    const double AbsoluteTolerance = sizeof(T) == 2 ? 5e-3 : 5e-2;
    for (size_t i = 0; i < N; ++i) {
      const double Expected = static_cast<double>(RefOutput[i]);
      const double Actual = static_cast<double>(Output[i]);
      if (!isclose(size_t(1), &Actual, &Expected, RelativeTolerance,
                   AbsoluteTolerance)) {
        printf("math mismatch at %zu: input=%a, input2=%a, expected=%a, "
               "actual=%a\n", i, static_cast<double>(Input1[i]),
               static_cast<double>(Input2[i]), Expected, Actual);
        return false;
      }
    }
    return true;
  }
};

// With 64 FP16 elements per 128-byte block, exercise two full blocks and a tail.
enum { N = 129 };

// Scalar math functions tests
template <typename T, float (*OP)(float)>
class ScalarUnaryMathTest : public TestBase<T> {
public:
  ScalarUnaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
    this->reset_output();
    prepare_unary_input<T, OP>(this->Input1);
    ref_unary_math<T, OP>(this->N, &this->RefOutput[0], &this->Input1[0]);
  }

  void run(unsigned) override {
    // Run scalar version (reference implementation)
    ref_unary_math<T, OP>(this->N, &this->Output[0], &this->Input1[0]);
  }
};

template <typename T, float (*OP)(float, float)>
class ScalarBinaryMathTest : public TestBase<T> {
public:
  ScalarBinaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
    this->reset_output();
    prepare_binary_input<T, OP>(this->Input1, this->Input2);
    ref_binary_math<T, OP>(this->N, &this->RefOutput[0], &this->Input1[0],
                           &this->Input2[0]);
  }

  void run(unsigned) override {
    // Run scalar version (reference implementation)
    ref_binary_math<T, OP>(this->N, &this->Output[0], &this->Input1[0],
                           &this->Input2[0]);
  }
};

// Ripple math functions tests
template <typename T, float (*REF_OP)(float), T (*RIPPLE_OP)(T)>
class RippleUnaryMathTest : public TestBase<T> {
public:
  RippleUnaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
    this->reset_output();
    prepare_unary_input<T, REF_OP>(this->Input1);
    ref_unary_math<T, REF_OP>(this->N, &this->RefOutput[0], &this->Input1[0]);
  }

  void run(unsigned) override {
    Ripple_unary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                    &this->Input1[0]);
  }
};

template <typename T, float (*REF_OP)(float, float), T (*RIPPLE_OP)(T, T)>
class RippleBinaryMathTest : public TestBase<T> {
public:
  RippleBinaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}
  void prepare(unsigned) override {
    this->reset_output();
    prepare_binary_input<T, REF_OP>(this->Input1, this->Input2);
    ref_binary_math<T, REF_OP>(this->N, &this->RefOutput[0], &this->Input1[0],
                               &this->Input2[0]);
  }
  void run(unsigned) override {
    Ripple_binary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                     &this->Input1[0], &this->Input2[0]);
  }
};

template <typename T, float (*REF_OP)(float), T (*RIPPLE_OP)(T)>
class RippleUnaryDomainTest : public TestBase<T> {
public:
  RippleUnaryDomainTest(TestFramework &TF) : TestBase<T>(TF, 128 / sizeof(T)) {}

  std::pair<bool, std::string> xfail() const override {
#if defined(__hexagon__) && __has_Float16__
    if (__is_same(T, _Float16) && REF_OP == acosf_fn)
      return {true, "The Hexagon HVX acosf16 implementation returns finite "
                    "values for inputs outside [-1, 1]."};
    if (__is_same(T, _Float16) && REF_OP == sqrtf_fn)
      return {true, "The Hexagon HVX sqrtf16 implementation returns NaN "
                    "for negative zero."};
#endif
    return {false, ""};
  }

  void prepare(unsigned) override {
    // Initialize to a value that cannot pass the special-value checks.
    this->reset_output(42.0f);
    prepare_unary_domain_input<T, REF_OP>(this->Input1);
    ref_unary_math<T, REF_OP>(this->N, &this->RefOutput[0],
                              &this->Input1[0]);
  }

  void run(unsigned) override {
    Ripple_unary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                    &this->Input1[0]);
  }

  bool verify() const override {
    for (size_t i = 0; i < this->N; ++i) {
      if (!same_special_value(this->Output[i], this->RefOutput[i])) {
        printf("domain mismatch at %zu: input=%a, expected=%a, actual=%a\n", i,
               static_cast<double>(this->Input1[i]),
               static_cast<double>(this->RefOutput[i]),
               static_cast<double>(this->Output[i]));
        return false;
      }
    }
    return true;
  }
};

template <typename T, float (*REF_OP)(float), T (*RIPPLE_OP)(T)>
class RippleUnaryZeroTest : public TestBase<T> {
public:
  RippleUnaryZeroTest(TestFramework &TF) : TestBase<T>(TF, 128 / sizeof(T)) {}

  std::pair<bool, std::string> xfail() const override {
#ifdef __hexagon__
    if (REF_OP == rsqrt_ref<float>)
      return {true, "The Hexagon HVX reciprocal square root implementation "
                    "does not return signed infinities for zero inputs."};
#endif
    return {false, ""};
  }

  void prepare(unsigned) override {
    this->reset_output(42.0f);
    prepare_zero_input(this->Input1);
    ref_unary_math<T, REF_OP>(this->N, &this->RefOutput[0],
                              &this->Input1[0]);
  }

  void run(unsigned) override {
    Ripple_unary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                    &this->Input1[0]);
  }

  bool verify() const override {
    for (size_t i = 0; i < this->N; ++i)
      if (!same_special_value(this->Output[i], this->RefOutput[i]))
        return false;
    return true;
  }
};

template <typename T, T (*RIPPLE_OP)(T, T)>
class RipplePowDomainTest : public TestBase<T> {
public:
  RipplePowDomainTest(TestFramework &TF) : TestBase<T>(TF, 128 / sizeof(T)) {}

  std::pair<bool, std::string> xfail() const override {
#ifdef __hexagon__
    return {true, "The Hexagon HVX power implementation does not always return "
                  "NaN for a negative base and fractional exponent."};
#else
    return {false, ""};
#endif
  }

  void prepare(unsigned) override {
    this->reset_output(42.0f);
    prepare_pow_domain_input(this->Input1, this->Input2);
    ref_binary_math<T, powf_fn>(this->N, &this->RefOutput[0],
                               &this->Input1[0], &this->Input2[0]);
  }

  void run(unsigned) override {
    Ripple_binary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                     &this->Input1[0], &this->Input2[0]);
  }

  bool verify() const override {
    for (size_t i = 0; i < this->N; ++i) {
      if (!same_special_value(this->Output[i], this->RefOutput[i])) {
        printf("domain mismatch at %zu: base=%a, exponent=%a, expected=%a, "
               "actual=%a\n",
               i, static_cast<double>(this->Input1[i]),
               static_cast<double>(this->Input2[i]),
               static_cast<double>(this->RefOutput[i]),
               static_cast<double>(this->Output[i]));
        return false;
      }
    }
    return true;
  }
};

// Unary float
DefineTest<ScalarUnaryMathTest<float, sinf_fn>>
    ScalarFloatSin("ScalarFloatSin");
DefineTest<RippleUnaryMathTest<float, sinf_fn, sinf_fn>>
    RippleFloatSin("RippleFloatSin");

DefineTest<ScalarUnaryMathTest<float, cosf_fn>>
    ScalarFloatCos("ScalarFloatCos");
DefineTest<RippleUnaryMathTest<float, cosf_fn, cosf_fn>>
    RippleFloatCos("RippleFloatCos");

DefineTest<ScalarUnaryMathTest<float, tanf_fn>>
    ScalarFloatTan("ScalarFloatTan");
DefineTest<RippleUnaryMathTest<float, tanf_fn, tanf_fn>>
    RippleFloatTan("RippleFloatTan");

DefineTest<ScalarUnaryMathTest<float, asinf_fn>>
    ScalarFloatAsin("ScalarFloatAsin");
DefineTest<RippleUnaryMathTest<float, asinf_fn, asinf_fn>>
    RippleFloatAsin("RippleFloatAsin");

DefineTest<ScalarUnaryMathTest<float, acosf_fn>>
    ScalarFloatAcos("ScalarFloatAcos");
DefineTest<RippleUnaryMathTest<float, acosf_fn, acosf_fn>>
    RippleFloatAcos("RippleFloatAcos");

DefineTest<ScalarUnaryMathTest<float, atanf_fn>>
    ScalarFloatAtan("ScalarFloatAtan");
DefineTest<RippleUnaryMathTest<float, atanf_fn, atanf_fn>>
    RippleFloatAtan("RippleFloatAtan");

DefineTest<ScalarUnaryMathTest<float, sinhf_fn>>
    ScalarFloatSinh("ScalarFloatSinh");
DefineTest<RippleUnaryMathTest<float, sinhf_fn, sinhf_fn>>
    RippleFloatSinh("RippleFloatSinh");

DefineTest<ScalarUnaryMathTest<float, coshf_fn>>
    ScalarFloatCosh("ScalarFloatCosh");
DefineTest<RippleUnaryMathTest<float, coshf_fn, coshf_fn>>
    RippleFloatCosh("RippleFloatCosh");

DefineTest<ScalarUnaryMathTest<float, tanhf_fn>>
    ScalarFloatTanh("ScalarFloatTanh");
DefineTest<RippleUnaryMathTest<float, tanhf_fn, tanhf_fn>>
    RippleFloatTanh("RippleFloatTanh");

DefineTest<ScalarUnaryMathTest<float, expf_fn>>
    ScalarFloatExp("ScalarFloatExp");
DefineTest<RippleUnaryMathTest<float, expf_fn, expf_fn>>
    RippleFloatExp("RippleFloatExp");

DefineTest<ScalarUnaryMathTest<float, logf_fn>>
    ScalarFloatLog("ScalarFloatLog");
DefineTest<RippleUnaryMathTest<float, logf_fn, logf_fn>>
    RippleFloatLog("RippleFloatLog");

DefineTest<ScalarUnaryMathTest<float, sqrtf_fn>>
    ScalarFloatSqrt("ScalarFloatSqrt");
DefineTest<RippleUnaryMathTest<float, sqrtf_fn, sqrtf_fn>>
    RippleFloatSqrt("RippleFloatSqrt");

DefineTest<ScalarUnaryMathTest<float, rsqrt_ref>>
    ScalarFloatRsqrt("ScalarFloatRsqrt");
DefineTest<RippleUnaryMathTest<float, rsqrt_ref, rsqrtf_fn>>
    RippleFloatRsqrt("RippleFloatRsqrt");

DefineTest<ScalarUnaryMathTest<float, truncf_fn>>
    ScalarFloatTrunc("ScalarFloatTrunc");
DefineTest<RippleUnaryMathTest<float, truncf_fn, truncf_fn>>
    RippleFloatTrunc("RippleFloatTrunc");

DefineTest<ScalarUnaryMathTest<float, roundf_fn>>
    ScalarFloatRound("ScalarFloatRound");
DefineTest<RippleUnaryMathTest<float, roundf_fn, roundf_fn>>
    RippleFloatRound("RippleFloatRound");

DefineTest<ScalarUnaryMathTest<float, floorf_fn>>
    ScalarFloatFloor("ScalarFloatFloor");
DefineTest<RippleUnaryMathTest<float, floorf_fn, floorf_fn>>
    RippleFloatFloor("RippleFloatFloor");

DefineTest<ScalarUnaryMathTest<float, ceilf_fn>>
    ScalarFloatCeil("ScalarFloatCeil");
DefineTest<RippleUnaryMathTest<float, ceilf_fn, ceilf_fn>>
    RippleFloatCeil("RippleFloatCeil");

// Use float scalar references for FP16 math.
#if __has_Float16__
DefineTest<ScalarUnaryMathTest<_Float16, sinf_fn>>
    ScalarHalfSin("ScalarHalfSin");
DefineTest<RippleUnaryMathTest<_Float16, sinf_fn, sinf16_fn>>
    RippleHalfSin("RippleHalfSin");

DefineTest<ScalarUnaryMathTest<_Float16, cosf_fn>>
    ScalarHalfCos("ScalarHalfCos");
DefineTest<RippleUnaryMathTest<_Float16, cosf_fn, cosf16_fn>>
    RippleHalfCos("RippleHalfCos");

DefineTest<ScalarUnaryMathTest<_Float16, tanf_fn>>
    ScalarHalfTan("ScalarHalfTan");
DefineTest<RippleUnaryMathTest<_Float16, tanf_fn, tanf16_fn>>
    RippleHalfTan("RippleHalfTan");

DefineTest<ScalarUnaryMathTest<_Float16, asinf_fn>>
    ScalarHalfAsin("ScalarHalfAsin");
DefineTest<RippleUnaryMathTest<_Float16, asinf_fn, asinf16_fn>>
    RippleHalfAsin("RippleHalfAsin");

DefineTest<ScalarUnaryMathTest<_Float16, acosf_fn>>
    ScalarHalfAcos("ScalarHalfAcos");
DefineTest<RippleUnaryMathTest<_Float16, acosf_fn, acosf16_fn>>
    RippleHalfAcos("RippleHalfAcos");

DefineTest<ScalarUnaryMathTest<_Float16, atanf_fn>>
    ScalarHalfAtan("ScalarHalfAtan");
DefineTest<RippleUnaryMathTest<_Float16, atanf_fn, atanf16_fn>>
    RippleHalfAtan("RippleHalfAtan");

DefineTest<ScalarUnaryMathTest<_Float16, sinhf_fn>>
    ScalarHalfSinh("ScalarHalfSinh");
DefineTest<RippleUnaryMathTest<_Float16, sinhf_fn, sinhf16_fn>>
    RippleHalfSinh("RippleHalfSinh");

DefineTest<ScalarUnaryMathTest<_Float16, coshf_fn>>
    ScalarHalfCosh("ScalarHalfCosh");
DefineTest<RippleUnaryMathTest<_Float16, coshf_fn, coshf16_fn>>
    RippleHalfCosh("RippleHalfCosh");

DefineTest<ScalarUnaryMathTest<_Float16, tanhf_fn>>
    ScalarHalfTanh("ScalarHalfTanh");
DefineTest<RippleUnaryMathTest<_Float16, tanhf_fn, tanhf16_fn>>
    RippleHalfTanh("RippleHalfTanh");

DefineTest<ScalarUnaryMathTest<_Float16, expf_fn>>
    ScalarHalfExp("ScalarHalfExp");
DefineTest<RippleUnaryMathTest<_Float16, expf_fn, expf16_fn>>
    RippleHalfExp("RippleHalfExp");

DefineTest<ScalarUnaryMathTest<_Float16, logf_fn>>
    ScalarHalfLog("ScalarHalfLog");
DefineTest<RippleUnaryMathTest<_Float16, logf_fn, logf16_fn>>
    RippleHalfLog("RippleHalfLog");

DefineTest<ScalarUnaryMathTest<_Float16, sqrtf_fn>>
    ScalarHalfSqrt("ScalarHalfSqrt");
DefineTest<RippleUnaryMathTest<_Float16, sqrtf_fn, sqrtf16_fn>>
    RippleHalfSqrt("RippleHalfSqrt");

DefineTest<ScalarUnaryMathTest<_Float16, rsqrt_ref<float>>>
    ScalarHalfRsqrt("ScalarHalfRsqrt");
DefineTest<RippleUnaryMathTest<_Float16, rsqrt_ref<float>, rsqrtf16_fn>>
    RippleHalfRsqrt("RippleHalfRsqrt");

DefineTest<ScalarUnaryMathTest<_Float16, truncf_fn>>
    ScalarHalfTrunc("ScalarHalfTrunc");
DefineTest<RippleUnaryMathTest<_Float16, truncf_fn, truncf16_fn>>
    RippleHalfTrunc("RippleHalfTrunc");

DefineTest<ScalarUnaryMathTest<_Float16, roundf_fn>>
    ScalarHalfRound("ScalarHalfRound");
DefineTest<RippleUnaryMathTest<_Float16, roundf_fn, roundf16_fn>>
    RippleHalfRound("RippleHalfRound");

DefineTest<ScalarUnaryMathTest<_Float16, floorf_fn>>
    ScalarHalfFloor("ScalarHalfFloor");
DefineTest<RippleUnaryMathTest<_Float16, floorf_fn, floorf16_fn>>
    RippleHalfFloor("RippleHalfFloor");

DefineTest<ScalarUnaryMathTest<_Float16, ceilf_fn>>
    ScalarHalfCeil("ScalarHalfCeil");
DefineTest<RippleUnaryMathTest<_Float16, ceilf_fn, ceilf16_fn>>
    RippleHalfCeil("RippleHalfCeil");
#endif

// Binary Float
DefineTest<ScalarBinaryMathTest<float, powf_fn>>
    ScalarFloatPow("ScalarFloatPow");
DefineTest<RippleBinaryMathTest<float, powf_fn, powf_fn>>
    RippleFloatPow("RippleFloatPow");

DefineTest<ScalarBinaryMathTest<float, hypot_ref>>
    ScalarFloatHypot("ScalarFloatHypot");
DefineTest<RippleBinaryMathTest<float, hypot_ref, hypotf_fn>>
    RippleFloatHypot("RippleFloatHypot");

// Domain behavior and signed special values are checked separately from the
// finite-range approximation tests above.
DefineTest<RippleUnaryDomainTest<float, asinf_fn, asinf_fn>>
    RippleFloatAsinDomain("RippleFloatAsinDomain");
DefineTest<RippleUnaryDomainTest<float, acosf_fn, acosf_fn>>
    RippleFloatAcosDomain("RippleFloatAcosDomain");
DefineTest<RippleUnaryDomainTest<float, logf_fn, logf_fn>>
    RippleFloatLogDomain("RippleFloatLogDomain");
DefineTest<RippleUnaryZeroTest<float, logf_fn, logf_fn>>
    RippleFloatLogZero("RippleFloatLogZero");
DefineTest<RippleUnaryDomainTest<float, sqrtf_fn, sqrtf_fn>>
    RippleFloatSqrtDomain("RippleFloatSqrtDomain");
DefineTest<RippleUnaryDomainTest<float, rsqrt_ref, rsqrtf_fn>>
    RippleFloatRsqrtDomain("RippleFloatRsqrtDomain");
DefineTest<RippleUnaryZeroTest<float, rsqrt_ref, rsqrtf_fn>>
    RippleFloatRsqrtZero("RippleFloatRsqrtZero");
DefineTest<RipplePowDomainTest<float, powf_fn>>
    RippleFloatPowDomain("RippleFloatPowDomain");

#if __has_Float16__
DefineTest<ScalarBinaryMathTest<_Float16, powf_fn>>
    ScalarHalfPow("ScalarHalfPow");
DefineTest<RippleBinaryMathTest<_Float16, powf_fn, powf16_fn>>
    RippleHalfPow("RippleHalfPow");

DefineTest<ScalarBinaryMathTest<_Float16, hypot_ref<float>>>
    ScalarHalfHypot("ScalarHalfHypot");
DefineTest<RippleBinaryMathTest<_Float16, hypot_ref<float>, hypotf16_fn>>
    RippleHalfHypot("RippleHalfHypot");

DefineTest<RippleUnaryDomainTest<_Float16, asinf_fn, asinf16_fn>>
    RippleHalfAsinDomain("RippleHalfAsinDomain");
DefineTest<RippleUnaryDomainTest<_Float16, acosf_fn, acosf16_fn>>
    RippleHalfAcosDomain("RippleHalfAcosDomain");
DefineTest<RippleUnaryDomainTest<_Float16, logf_fn, logf16_fn>>
    RippleHalfLogDomain("RippleHalfLogDomain");
DefineTest<RippleUnaryDomainTest<_Float16, sqrtf_fn, sqrtf16_fn>>
    RippleHalfSqrtDomain("RippleHalfSqrtDomain");
DefineTest<RippleUnaryDomainTest<_Float16, rsqrt_ref<float>, rsqrtf16_fn>>
    RippleHalfRsqrtDomain("RippleHalfRsqrtDomain");
DefineTest<RippleUnaryZeroTest<_Float16, logf_fn, logf16_fn>>
    RippleHalfLogZero("RippleHalfLogZero");
DefineTest<RippleUnaryZeroTest<_Float16, rsqrt_ref<float>, rsqrtf16_fn>>
    RippleHalfRsqrtZero("RippleHalfRsqrtZero");
DefineTest<RipplePowDomainTest<_Float16, powf16_fn>>
    RippleHalfPowDomain("RippleHalfPowDomain");
#endif

#if __has_bf16__
DefineTest<ScalarUnaryMathTest<__bf16, sinf_fn>>
    ScalarBFloatSin("ScalarBFloatSin");
DefineTest<ScalarUnaryMathTest<__bf16, cosf_fn>>
    ScalarBFloatCos("ScalarBFloatCos");
DefineTest<ScalarUnaryMathTest<__bf16, tanf_fn>>
    ScalarBFloatTan("ScalarBFloatTan");
DefineTest<ScalarUnaryMathTest<__bf16, asinf_fn>>
    ScalarBFloatAsin("ScalarBFloatAsin");
DefineTest<ScalarUnaryMathTest<__bf16, acosf_fn>>
    ScalarBFloatAcos("ScalarBFloatAcos");
DefineTest<ScalarUnaryMathTest<__bf16, atanf_fn>>
    ScalarBFloatAtan("ScalarBFloatAtan");
DefineTest<ScalarUnaryMathTest<__bf16, sinhf_fn>>
    ScalarBFloatSinh("ScalarBFloatSinh");
DefineTest<ScalarUnaryMathTest<__bf16, coshf_fn>>
    ScalarBFloatCosh("ScalarBFloatCosh");
DefineTest<ScalarUnaryMathTest<__bf16, tanhf_fn>>
    ScalarBFloatTanh("ScalarBFloatTanh");
DefineTest<ScalarUnaryMathTest<__bf16, expf_fn>>
    ScalarBFloatExp("ScalarBFloatExp");
DefineTest<ScalarUnaryMathTest<__bf16, logf_fn>>
    ScalarBFloatLog("ScalarBFloatLog");
DefineTest<ScalarUnaryMathTest<__bf16, sqrtf_fn>>
    ScalarBFloatSqrt("ScalarBFloatSqrt");
DefineTest<ScalarUnaryMathTest<__bf16, rsqrt_ref<float>>>
    ScalarBFloatRsqrt("ScalarBFloatRsqrt");
DefineTest<ScalarUnaryMathTest<__bf16, truncf_fn>>
    ScalarBFloatTrunc("ScalarBFloatTrunc");
DefineTest<ScalarUnaryMathTest<__bf16, roundf_fn>>
    ScalarBFloatRound("ScalarBFloatRound");
DefineTest<ScalarUnaryMathTest<__bf16, floorf_fn>>
    ScalarBFloatFloor("ScalarBFloatFloor");
DefineTest<ScalarUnaryMathTest<__bf16, ceilf_fn>>
    ScalarBFloatCeil("ScalarBFloatCeil");
DefineTest<ScalarBinaryMathTest<__bf16, powf_fn>>
    ScalarBFloatPow("ScalarBFloatPow");
DefineTest<ScalarBinaryMathTest<__bf16, hypot_ref<float>>>
    ScalarBFloatHypot("ScalarBFloatHypot");

DefineTest<RippleUnaryMathTest<__bf16, sinf_fn, sinbf16_fn>>
    RippleBFloatSin("RippleBFloatSin");
DefineTest<RippleUnaryMathTest<__bf16, cosf_fn, cosbf16_fn>>
    RippleBFloatCos("RippleBFloatCos");
DefineTest<RippleUnaryMathTest<__bf16, tanf_fn, tanbf16_fn>>
    RippleBFloatTan("RippleBFloatTan");
DefineTest<RippleUnaryMathTest<__bf16, asinf_fn, asinbf16_fn>>
    RippleBFloatAsin("RippleBFloatAsin");
DefineTest<RippleUnaryMathTest<__bf16, acosf_fn, acosbf16_fn>>
    RippleBFloatAcos("RippleBFloatAcos");
DefineTest<RippleUnaryMathTest<__bf16, atanf_fn, atanbf16_fn>>
    RippleBFloatAtan("RippleBFloatAtan");
DefineTest<RippleUnaryMathTest<__bf16, sinhf_fn, sinhbf16_fn>>
    RippleBFloatSinh("RippleBFloatSinh");
DefineTest<RippleUnaryMathTest<__bf16, coshf_fn, coshbf16_fn>>
    RippleBFloatCosh("RippleBFloatCosh");
DefineTest<RippleUnaryMathTest<__bf16, tanhf_fn, tanhbf16_fn>>
    RippleBFloatTanh("RippleBFloatTanh");
DefineTest<RippleUnaryMathTest<__bf16, expf_fn, expbf16_fn>>
    RippleBFloatExp("RippleBFloatExp");
DefineTest<RippleUnaryMathTest<__bf16, logf_fn, logbf16_fn>>
    RippleBFloatLog("RippleBFloatLog");
DefineTest<RippleUnaryMathTest<__bf16, sqrtf_fn, sqrtbf16_fn>>
    RippleBFloatSqrt("RippleBFloatSqrt");
DefineTest<RippleUnaryMathTest<__bf16, rsqrt_ref<float>, rsqrtbf16_fn>>
    RippleBFloatRsqrt("RippleBFloatRsqrt");
DefineTest<RippleUnaryMathTest<__bf16, truncf_fn, truncbf16_fn>>
    RippleBFloatTrunc("RippleBFloatTrunc");
DefineTest<RippleUnaryMathTest<__bf16, roundf_fn, roundbf16_fn>>
    RippleBFloatRound("RippleBFloatRound");
DefineTest<RippleUnaryMathTest<__bf16, floorf_fn, floorbf16_fn>>
    RippleBFloatFloor("RippleBFloatFloor");
DefineTest<RippleUnaryMathTest<__bf16, ceilf_fn, ceilbf16_fn>>
    RippleBFloatCeil("RippleBFloatCeil");
DefineTest<RippleBinaryMathTest<__bf16, powf_fn, powbf16_fn>>
    RippleBFloatPow("RippleBFloatPow");
DefineTest<RippleBinaryMathTest<__bf16, hypot_ref<float>, hypotbf16_fn>>
    RippleBFloatHypot("RippleBFloatHypot");

DefineTest<RippleUnaryDomainTest<__bf16, asinf_fn, asinbf16_fn>>
    RippleBFloatAsinDomain("RippleBFloatAsinDomain");
DefineTest<RippleUnaryDomainTest<__bf16, acosf_fn, acosbf16_fn>>
    RippleBFloatAcosDomain("RippleBFloatAcosDomain");
DefineTest<RippleUnaryDomainTest<__bf16, logf_fn, logbf16_fn>>
    RippleBFloatLogDomain("RippleBFloatLogDomain");
DefineTest<RippleUnaryDomainTest<__bf16, sqrtf_fn, sqrtbf16_fn>>
    RippleBFloatSqrtDomain("RippleBFloatSqrtDomain");
DefineTest<RippleUnaryDomainTest<__bf16, rsqrt_ref<float>, rsqrtbf16_fn>>
    RippleBFloatRsqrtDomain("RippleBFloatRsqrtDomain");
DefineTest<RippleUnaryZeroTest<__bf16, logf_fn, logbf16_fn>>
    RippleBFloatLogZero("RippleBFloatLogZero");
DefineTest<RippleUnaryZeroTest<__bf16, rsqrt_ref<float>, rsqrtbf16_fn>>
    RippleBFloatRsqrtZero("RippleBFloatRsqrtZero");
DefineTest<RipplePowDomainTest<__bf16, powbf16_fn>>
    RippleBFloatPowDomain("RippleBFloatPowDomain");
#endif

} // namespace
