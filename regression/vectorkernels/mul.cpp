//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <cmath>
#include <cstddef>

#include "scalar_reference.h"
#include <ripple-test-suite/ripple-test-suite.h>
#include <ripple.h>

namespace {

static constexpr int N = 400;

template <typename T>
void mul_ripple(T *__restrict__ pA, T *__restrict__ pB, T *__restrict__ pC) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  ripple_parallel(BS, 0);
  for (int i = 0; i < N; i++) {
    pA[i] = pB[i] * pC[i];
  }
}

template <typename T>
void mul_pred_ripple(T *__restrict__ pA, T *__restrict__ pB,
                     T *__restrict__ pC) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  ripple_parallel(BS, 0);
  for (int i = 0; i < N; i++) {
    if (pB[i] > (T)N)
      pA[i] = pB[i] * pC[i];
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class MulTest : public Test {
  std::vector<T> A, B, C, ARef;

public:
  MulTest(TestFramework &TF) : Test(TF), A(N, 0), B(N, 0), C(N, 0), ARef(N, 0) {
    for (int i = 0; i < N; i++) {
      B[i] = static_cast<T>(100.0 * (randn() - 0.5));
      C[i] = static_cast<T>(100.0 * (randn() - 0.5));
    }
    scalar_reference::transform(&ARef[0], &B[0], &C[0], N,
                                [](T lhs, T rhs) { return lhs * rhs; });
  }

  void run(unsigned) override { mul_ripple(&A[0], &B[0], &C[0]); }

  bool verify() const override {
    return isclose(N, &A[0], &ARef[0], pow(0.1, SigDigits));
  }
};

template <typename T, unsigned SigDigits> class MulPredTest : public Test {
  std::vector<T> A, B, C, ARef;

public:
  MulPredTest(TestFramework &TF)
      : Test(TF), A(N, 0), B(N, 0), C(N, 0), ARef(N, 0) {
    for (int i = 0; i < N; i++) {
      B[i] = static_cast<T>(1000.0 * randn());
      C[i] = static_cast<T>(100.0 * (randn() - 0.5));
    }
    scalar_reference::transform_if(
        &ARef[0], &B[0], &C[0], N,
        [](T lhs) { return lhs > static_cast<T>(N); },
        [](T lhs, T rhs) { return lhs * rhs; });
  }

  void run(unsigned) override { mul_pred_ripple(&A[0], &B[0], &C[0]); }

  bool verify() const override {
    return isclose(N, &A[0], &ARef[0], pow(0.1, SigDigits));
  }
};

DefineTest<MulTest<int, 0>> TMulInt("Mul<int>");
DefineTest<MulTest<float, 4>> TMulFloat("Mul<float>");
DefineTest<MulPredTest<int, 0>> TMulPredInt("MulPred<int>");
DefineTest<MulPredTest<float, 4>> TMulPredFloat("MulPred<float>");

} // namespace
