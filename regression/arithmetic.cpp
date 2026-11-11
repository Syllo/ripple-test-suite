//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple-test-suite/ripple-test-suite.h>

#include <ripple.h>
#include <ripple_math.h>

#include <limits>

namespace {

/**
 * @brief Adds the vectors "a", "b" and populates "apb" with the results.
 *
 * @tparam T
 * @param N
 * @param a
 * @param b
 */
template <typename T>
void add_ripple(size_t N, const T *a, const T *b, T *apb) {
  auto BS = ripple_set_block_shape(0, 128 / sizeof(T));
  ripple_parallel(BS, 0);
  for (size_t i = 0; i < N; i++)
    apb[i] = a[i] + b[i];
}

/**
 * @brief Subtracts the vectors "b" from "a" and populates "asb" with the
 * results.
 *
 * @tparam T
 * @param N
 * @param a
 * @param b
 */
template <typename T>
void sub_ripple(size_t N, const T *a, const T *b, T *asb) {
  auto BS = ripple_set_block_shape(0, 128 / sizeof(T));
  ripple_parallel(BS, 0);
  for (size_t i = 0; i < N; i++)
    asb[i] = a[i] - b[i];
}

/**
 * @brief Multiplies the vectors "a" and "b" and populates "atb" with the
 * results.
 *
 * @tparam T
 * @param N
 * @param a
 * @param b
 */
template <typename T>
void mult_ripple(size_t N, const T *a, const T *b, T *atb) {
  auto BS = ripple_set_block_shape(0, 128 / sizeof(T));
  ripple_parallel(BS, 0);
  for (size_t i = 0; i < N; i++)
    atb[i] = a[i] * b[i];
}

using namespace ripple_test_suite;

enum Op { ADD, SUB, MULT };

template <typename T, Op O, unsigned SigDigits> class OpTest : public Test {
  static constexpr int N = 1729;
  std::vector<T> X, Y, XopY, XopYRef;

public:
  OpTest(TestFramework &TestFramework)
      : Test(TestFramework), X(N, 0), Y(N, 0), XopY(N, 0), XopYRef(N, 0) {
    for (int i = 0; i < N; i++) {
      X[i] = static_cast<T>(42 * (randn() - 0.5));
      Y[i] = static_cast<T>(42 * (randn() - 0.5));
      if (O == Op::ADD && __is_same(T, float)) {
        // Test near cancellation separately in AddCancellationTest. Keep
        // both signs here, with sums bounded away from cancellation.
        const double A = static_cast<double>(X[i]);
        const double B = static_cast<double>(Y[i]);
        if (__builtin_fabs(A + B) <
            0.25 * (__builtin_fabs(A) + __builtin_fabs(B)))
          Y[i] = -Y[i];
      }
      if (O == Op::ADD)
        XopYRef[i] = X[i] + Y[i];
      if (O == Op::SUB)
        XopYRef[i] = X[i] - Y[i];
      if (O == Op::MULT)
        XopYRef[i] = X[i] * Y[i];
    }
  }

  void run(unsigned) override {
    if (O == Op::ADD)
      add_ripple(N, &(X[0]), &(Y[0]), &(XopY[0]));
    if (O == Op::SUB)
      sub_ripple(N, &(X[0]), &(Y[0]), &(XopY[0]));
    if (O == Op::MULT)
      mult_ripple(N, &(X[0]), &(Y[0]), &(XopY[0]));
  }

  bool verify() const override {
    return isclose(N, &(XopY[0]), &(XopYRef[0]), pow(0.1, SigDigits));
  }
};

class AddCancellationTest : public Test {
  static constexpr size_t N = 129;
  std::vector<float> X, Y, Output, Expected;

public:
  AddCancellationTest(TestFramework &TF)
      : Test(TF), X(N), Y(N), Output(N), Expected(N) {
    // 1 - 2^-24 is the FP32 value immediately below 1. Legacy QFloat
    // addition doubles the small residual when these operands cancel.
    const float Step = 1.0f / (1u << 24);
    for (size_t i = 0; i < N; ++i) {
      const float Scale = static_cast<float>(1u << (i % 5));
      const float Sign = i % 2 ? -1.0f : 1.0f;
      X[i] = Sign * Scale;
      Y[i] = -Sign * Scale * (1.0f - Step);
      Expected[i] = Sign * Scale * Step;
      if (i % 4 >= 2)
        std::swap(X[i], Y[i]);
    }
  }

  std::pair<bool, std::string> xfail() const override {
#if defined(__hexagon__) && defined(__HVX__) && !defined(__HVX_IEEE_FP__) && \
    (__HEXAGON_ARCH__ == 68 || __HEXAGON_ARCH__ == 69 || \
     __HEXAGON_ARCH__ == 73 || __HEXAGON_ARCH__ == 75)
    return {true, "Legacy Hexagon QFloat FP32 addition loses accuracy near "
                  "cancellation."};
#endif
    return {false, ""};
  }

  void prepare(unsigned) override {
    std::fill(Output.begin(), Output.end(), 42.0f);
  }

  void run(unsigned) override {
    add_ripple(N, &X[0], &Y[0], &Output[0]);
  }

  bool verify() const override {
    return isclose(N, &Output[0], &Expected[0], 1e-4);
  }
};

template <typename T> class AddOverflowTest : public Test {
  static constexpr size_t N = 129;
  std::vector<T> X, Y, Output, Expected;

public:
  AddOverflowTest(TestFramework &TF)
      : Test(TF), X(N), Y(N), Output(N), Expected(N) {
    for (size_t i = 0; i < N; ++i) {
      const T Max = std::numeric_limits<T>::max();
      const T Inf = std::numeric_limits<T>::infinity();
      X[i] = Y[i] = i % 2 ? -Max : Max;
      Expected[i] = i % 2 ? -Inf : Inf;
    }
  }

  std::pair<bool, std::string> xfail() const override {
#if defined(__hexagon__) && defined(__HVX__) && !defined(__HVX_IEEE_FP__) && \
    (__HEXAGON_ARCH__ == 68 || __HEXAGON_ARCH__ == 69 || \
     __HEXAGON_ARCH__ == 73 || __HEXAGON_ARCH__ == 75)
    if (__is_same(T, float))
      return {true, "Legacy Hexagon QFloat addition returns NaN instead of "
                    "signed infinity on FP32 overflow."};
#endif
    return {false, ""};
  }

  void prepare(unsigned) override {
    std::fill(Output.begin(), Output.end(), T(42));
  }

  void run(unsigned) override {
    add_ripple(N, &X[0], &Y[0], &Output[0]);
  }

  bool verify() const override {
    // isclose checks infinity classification and sign before finite tolerance.
    return isclose(N, &Output[0], &Expected[0]);
  }
};

// #if __has_Float16__
// DefineTest<OpTest<_Float16, Op::ADD, 3>> TF16Add("OpTest<+,f16>");
// DefineTest<OpTest<_Float16, Op::SUB, 3>> TF16Sub("OpTest<-,f16>");
// DefineTest<OpTest<_Float16, Op::MULT, 3>> TF16Mult("OpTest<*,f16>");
// #endif

DefineTest<OpTest<float, Op::ADD, 4>> TF32Add("OpTest<+,f32>");
DefineTest<OpTest<float, Op::SUB, 4>> TF32Sub("OpTest<-,f32>");
DefineTest<OpTest<float, Op::MULT, 4>> TF32Mult("OpTest<*,f32>");

DefineTest<OpTest<double, Op::ADD, 6>> TF64Add("OpTest<+,f64>");
DefineTest<OpTest<double, Op::SUB, 6>> TF64Sub("OpTest<-,f64>");
DefineTest<OpTest<double, Op::MULT, 6>> TF64Mult("OpTest<*,f64>");

DefineTest<AddCancellationTest> TF32AddCancellation("OpTest<+,f32,cancellation>");
DefineTest<AddOverflowTest<float>> TF32AddOverflow("OpTest<+,f32,overflow>");
DefineTest<AddOverflowTest<double>> TF64AddOverflow("OpTest<+,f64,overflow>");

#ifdef __hexagon__
// TODO: Fix this for x86.
#if __has_bf16__
DefineTest<OpTest<__bf16, Op::ADD, 3>> TBF16Add("OpTest<+,bf16>");
DefineTest<OpTest<__bf16, Op::SUB, 3>> TBF16Sub("OpTest<-,bf16>");
DefineTest<OpTest<__bf16, Op::MULT, 3>> TBF16Mult("OpTest<*,bf16>");
#endif
#endif

// TODO: Test NaN/infinity operands, signed zeros, and subnormals.

} // namespace
