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

template <typename T, T (*OP)(T)>
void prepare_unary_input(std::vector<T> &Input) {
  if (OP == asinf_fn || OP == acosf_fn || OP == tanf_fn) {
    fill_linear(Input, -1.0, 1.0);
  } else if (OP == expf_fn || OP == sinhf_fn || OP == coshf_fn) {
    fill_linear(Input, -4.0, 4.0);
  } else if (OP == logf_fn || OP == rsqrt_ref<T>) {
    fill_linear(Input, 0.125, 16.0);
  } else if (OP == sqrtf_fn) {
    fill_linear(Input, 0.0, 16.0);
  } else {
    fill_linear(Input, -8.0, 8.0);
  }
}

template <typename T, T (*OP)(T, T)>
void prepare_binary_input(std::vector<T> &Input1, std::vector<T> &Input2) {
  if (OP == powf_fn) {
    fill_linear(Input1, 0.5, 2.0);
    fill_linear(Input2, 0.0, 1.0);
  } else {
    fill_linear(Input1, -8.0, 8.0);
    fill_linear(Input2, 4.0, -4.0);
  }
}

template <size_t NumValues>
void repeat_values(std::vector<float> &Input,
                   const float (&Values)[NumValues]) {
  for (size_t i = 0; i < Input.size(); ++i)
    Input[i] = Values[i % NumValues];
}

template <float (*OP)(float)>
void prepare_unary_domain_input(std::vector<float> &Input) {
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

void prepare_zero_input(std::vector<float> &Input) {
  const float Values[] = {-0.0f, 0.0f};
  repeat_values(Input, Values);
}

void prepare_pow_domain_input(std::vector<float> &Input1,
                              std::vector<float> &Input2) {
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

template <typename T, T (*OP)(T)>
void ref_unary_math(size_t N, T *output, const T *input) {
  for (size_t i = 0; i < N; i++)
    output[i] = OP(input[i]);
}

template <typename T, T (*OP)(T, T)>
void ref_binary_math(size_t N, T *output, const T *input1, const T *input2) {
  for (size_t i = 0; i < N; i++)
    output[i] = OP(input1[i], input2[i]);
}

template <typename T> class TestBase : public Test {
protected:
  const size_t N;
  std::vector<T> Input1, Input2, Output;
  std::vector<T> RefOutput;

public:
  TestBase(TestFramework &TF, size_t N)
      : Test(TF), N(N), Input1(N), Output(N), Input2(N), RefOutput(N) {}

  bool verify() const override {
    return isclose(N, &RefOutput[0], &Output[0], 1e-5, 5 * 1e-2);
  }
};

enum { N = 129 };
enum { DomainN = 32 };

// Scalar math functions tests
template <typename T, T (*OP)(T)>
class ScalarUnaryMathTest : public TestBase<T> {
public:
  ScalarUnaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
    prepare_unary_input<T, OP>(this->Input1);
    ref_unary_math<T, OP>(this->N, &this->RefOutput[0], &this->Input1[0]);
  }

  void run(unsigned) override {
    // Run scalar version (reference implementation)
    ref_unary_math<T, OP>(this->N, &this->Output[0], &this->Input1[0]);
  }
};

template <typename T, T (*OP)(T, T)>
class ScalarBinaryMathTest : public TestBase<T> {
public:
  ScalarBinaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
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
template <typename T, T (*REF_OP)(T), T (*RIPPLE_OP)(T)>
class RippleUnaryMathTest : public TestBase<T> {
public:
  RippleUnaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
    prepare_unary_input<T, REF_OP>(this->Input1);
    ref_unary_math<T, REF_OP>(this->N, &this->RefOutput[0], &this->Input1[0]);
  }

  void run(unsigned) override {
    Ripple_unary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                    &this->Input1[0]);
  }
};

template <typename T, T (*REF_OP)(T, T), T (*RIPPLE_OP)(T, T)>
class RippleBinaryMathTest : public TestBase<T> {
public:
  RippleBinaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}
  void prepare(unsigned) override {
    prepare_binary_input<T, REF_OP>(this->Input1, this->Input2);
    ref_binary_math<T, REF_OP>(this->N, &this->RefOutput[0], &this->Input1[0],
                               &this->Input2[0]);
  }
  void run(unsigned) override {
    Ripple_binary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                     &this->Input1[0], &this->Input2[0]);
  }
};

template <float (*REF_OP)(float), float (*RIPPLE_OP)(float)>
class RippleUnaryDomainTest : public TestBase<float> {
public:
  RippleUnaryDomainTest(TestFramework &TF) : TestBase<float>(TF, DomainN) {}

  void prepare(unsigned) override {
    prepare_unary_domain_input<REF_OP>(this->Input1);
    ref_unary_math<float, REF_OP>(this->N, &this->RefOutput[0],
                                  &this->Input1[0]);
  }

  void run(unsigned) override {
    Ripple_unary_math<float, RIPPLE_OP>(this->N, &this->Output[0],
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

template <float (*REF_OP)(float), float (*RIPPLE_OP)(float)>
class RippleUnaryZeroTest : public TestBase<float> {
public:
  RippleUnaryZeroTest(TestFramework &TF) : TestBase<float>(TF, DomainN) {}

  std::pair<bool, std::string> xfail() const override {
#ifdef __hexagon__
    if (REF_OP == logf_fn)
      return {true, "The Hexagon HVX logf implementation does not return "
                    "-infinity for zero inputs."};
    return {true, "The Hexagon HVX rsqrtf implementation does not return "
                  "signed infinities for zero inputs."};
#else
    return {false, ""};
#endif
  }

  void prepare(unsigned) override {
    prepare_zero_input(this->Input1);
    ref_unary_math<float, REF_OP>(this->N, &this->RefOutput[0],
                                  &this->Input1[0]);
  }

  void run(unsigned) override {
    Ripple_unary_math<float, RIPPLE_OP>(this->N, &this->Output[0],
                                        &this->Input1[0]);
  }

  bool verify() const override {
    for (size_t i = 0; i < this->N; ++i)
      if (!same_special_value(this->Output[i], this->RefOutput[i]))
        return false;
    return true;
  }
};

class RipplePowDomainTest : public TestBase<float> {
public:
  RipplePowDomainTest(TestFramework &TF) : TestBase<float>(TF, DomainN) {}

  std::pair<bool, std::string> xfail() const override {
#ifdef __hexagon__
    return {true, "The Hexagon HVX powf implementation does not always return "
                  "NaN for a negative base and fractional exponent."};
#else
    return {false, ""};
#endif
  }

  void prepare(unsigned) override {
    prepare_pow_domain_input(this->Input1, this->Input2);
    ref_binary_math<float, powf_fn>(this->N, &this->RefOutput[0],
                                    &this->Input1[0], &this->Input2[0]);
  }

  void run(unsigned) override {
    Ripple_binary_math<float, powf_fn>(this->N, &this->Output[0],
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

// FP16 coverage remains disabled because the Hexagon runtime does not provide
// the scalar FP16 math entry points needed by the reference and loop remainder.
#if 0
DefineTest<ScalarUnaryMathTest<_Float16, sinf16_fn>>
    ScalarHalfSin("ScalarHalfSin");
DefineTest<RippleUnaryMathTest<_Float16, sinf16_fn, sinf16_fn>>
    RippleHalfSin("RippleHalfSin");

DefineTest<ScalarUnaryMathTest<_Float16, cosf16_fn>>
    ScalarHalfCos("ScalarHalfCos");
DefineTest<RippleUnaryMathTest<_Float16, cosf16_fn, cosf16_fn>>
    RippleHalfCos("RippleHalfCos");

DefineTest<ScalarUnaryMathTest<_Float16, tanf16_fn>>
    ScalarHalfTan("ScalarHalfTan");
DefineTest<RippleUnaryMathTest<_Float16, tanf16_fn, tanf16_fn>>
    RippleHalfTan("RippleHalfTan");

DefineTest<ScalarUnaryMathTest<_Float16, asinf16_fn>>
    ScalarHalfAsin("ScalarHalfAsin");
DefineTest<RippleUnaryMathTest<_Float16, asinf16_fn, asinf16_fn>>
    RippleHalfAsin("RippleHalfAsin");

DefineTest<ScalarUnaryMathTest<_Float16, acosf16_fn>>
    ScalarHalfAcos("ScalarHalfAcos");
DefineTest<RippleUnaryMathTest<_Float16, acosf16_fn, acosf16_fn>>
    RippleHalfAcos("RippleHalfAcos");

DefineTest<ScalarUnaryMathTest<_Float16, atanf16_fn>>
    ScalarHalfAtan("ScalarHalfAtan");
DefineTest<RippleUnaryMathTest<_Float16, atanf16_fn, atanf16_fn>>
    RippleHalfAtan("RippleHalfAtan");

DefineTest<ScalarUnaryMathTest<_Float16, sinhf16_fn>>
    ScalarHalfSinh("ScalarHalfSinh");
DefineTest<RippleUnaryMathTest<_Float16, sinhf16_fn, sinhf16_fn>>
    RippleHalfSinh("RippleHalfSinh");

DefineTest<ScalarUnaryMathTest<_Float16, coshf16_fn>>
    ScalarHalfCosh("ScalarHalfCosh");
DefineTest<RippleUnaryMathTest<_Float16, coshf16_fn, coshf16_fn>>
    RippleHalfCosh("RippleHalfCosh");

DefineTest<ScalarUnaryMathTest<_Float16, tanhf16_fn>>
    ScalarHalfTanh("ScalarHalfTanh");
DefineTest<RippleUnaryMathTest<_Float16, tanhf16_fn, tanhf16_fn>>
    RippleHalfTanh("RippleHalfTanh");

DefineTest<ScalarUnaryMathTest<_Float16, expf16_fn>>
    ScalarHalfExp("ScalarHalfExp");
DefineTest<RippleUnaryMathTest<_Float16, expf16_fn, expf16_fn>>
    RippleHalfExp("RippleHalfExp");

DefineTest<ScalarUnaryMathTest<_Float16, logf16_fn>>
    ScalarHalfLog("ScalarHalfLog");
DefineTest<RippleUnaryMathTest<_Float16, logf16_fn, logf16_fn>>
    RippleHalfLog("RippleHalfLog");

DefineTest<ScalarUnaryMathTest<_Float16, sqrtf16_fn>>
    ScalarHalfSqrt("ScalarHalfSqrt");
DefineTest<RippleUnaryMathTest<_Float16, sqrtf16_fn, sqrtf16_fn>>
    RippleHalfSqrt("RippleHalfSqrt");

DefineTest<ScalarUnaryMathTest<_Float16, rsqrt_ref>>
    ScalarHalfRsqrt("ScalarHalfRsqrt");
DefineTest<RippleUnaryMathTest<_Float16, rsqrt_ref, rsqrtf16_fn>>
    RippleHalfRsqrt("RippleHalfRsqrt");

DefineTest<ScalarUnaryMathTest<_Float16, truncf16_fn>>
    ScalarHalfTrunc("ScalarHalfTrunc");
DefineTest<RippleUnaryMathTest<_Float16, truncf16_fn, truncf16_fn>>
    RippleHalfTrunc("RippleHalfTrunc");

DefineTest<ScalarUnaryMathTest<_Float16, roundf16_fn>>
    ScalarHalfRound("ScalarHalfRound");
DefineTest<RippleUnaryMathTest<_Float16, roundf16_fn, roundf16_fn>>
    RippleHalfRound("RippleHalfRound");

DefineTest<ScalarUnaryMathTest<_Float16, floorf16_fn>>
    ScalarHalfFloor("ScalarHalfFloor");
DefineTest<RippleUnaryMathTest<_Float16, floorf16_fn, floorf16_fn>>
    RippleHalfFloor("RippleHalfFloor");

DefineTest<ScalarUnaryMathTest<_Float16, ceilf16_fn>>
    ScalarHalfCeil("ScalarHalfCeil");
DefineTest<RippleUnaryMathTest<_Float16, ceilf16_fn, ceilf16_fn>>
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
DefineTest<RippleUnaryDomainTest<asinf_fn, asinf_fn>>
    RippleFloatAsinDomain("RippleFloatAsinDomain");
DefineTest<RippleUnaryDomainTest<acosf_fn, acosf_fn>>
    RippleFloatAcosDomain("RippleFloatAcosDomain");
DefineTest<RippleUnaryDomainTest<logf_fn, logf_fn>>
    RippleFloatLogDomain("RippleFloatLogDomain");
DefineTest<RippleUnaryZeroTest<logf_fn, logf_fn>>
    RippleFloatLogZero("RippleFloatLogZero");
DefineTest<RippleUnaryDomainTest<sqrtf_fn, sqrtf_fn>>
    RippleFloatSqrtDomain("RippleFloatSqrtDomain");
DefineTest<RippleUnaryDomainTest<rsqrt_ref, rsqrtf_fn>>
    RippleFloatRsqrtDomain("RippleFloatRsqrtDomain");
DefineTest<RippleUnaryZeroTest<rsqrt_ref, rsqrtf_fn>>
    RippleFloatRsqrtZero("RippleFloatRsqrtZero");
DefineTest<RipplePowDomainTest> RippleFloatPowDomain("RippleFloatPowDomain");

#if 0
DefineTest<ScalarBinaryMathTest<_Float16, powf16_fn>>
    ScalarHalfPow("ScalarHalfPow");
DefineTest<RippleBinaryMathTest<_Float16, powf16_fn, powf16_fn>>
    RippleHalfPow("RippleHalfPow");

DefineTest<ScalarBinaryMathTest<_Float16, hypot_ref>>
    ScalarHalfHypot("ScalarHalfHypot");
DefineTest<RippleBinaryMathTest<_Float16, hypot_ref, hypotf16_fn>>
    RippleHalfHypot("RippleHalfHypot");
#endif

} // namespace
