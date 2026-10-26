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

template <typename T> void step_ripple(T *__restrict__ pA, T *__restrict__ pB) {
  T Threshold = (T)50;
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  ripple_parallel(BS, 0);
  for (int i = 0; i < N; i++) {
    pA[i] = pB[i] > Threshold ? (T)1 : (T)0;
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class StepTest : public Test {
  std::vector<T> A, B, ARef;

public:
  StepTest(TestFramework &TF) : Test(TF), A(N, 0), B(N, 0), ARef(N, 0) {
    for (int i = 0; i < N; i++)
      B[i] = static_cast<T>(200.0 * randn());
    scalar_reference::transform(&ARef[0], &B[0], N, [](T value) {
      return value > static_cast<T>(50) ? static_cast<T>(1) : T{};
    });
  }

  void run(unsigned) override { step_ripple(&A[0], &B[0]); }

  bool verify() const override {
    return isclose(N, &A[0], &ARef[0], pow(0.1, SigDigits));
  }
};

DefineTest<StepTest<int, 0>> TStepInt("Step<int>");
DefineTest<StepTest<float, 4>> TStepFloat("Step<float>");

} // namespace
