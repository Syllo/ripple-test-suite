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

static constexpr int DIM = 20;

template <typename T>
void outer_ripple(T *__restrict__ pA, T *__restrict__ pB, T *__restrict__ pC) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  ripple_parallel(BS, 0);
  for (int k = 0; k < DIM * DIM; k++) {
    int r = k / DIM;
    int c = k % DIM;
    pA[k] = pB[r] * pC[c];
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class OuterTest : public Test {
  static constexpr int SZ = DIM * DIM;
  std::vector<T> A, B, C, ARef;

public:
  OuterTest(TestFramework &TF)
      : Test(TF), A(SZ, 0), B(DIM, 0), C(DIM, 0), ARef(SZ, 0) {
    for (int i = 0; i < DIM; i++) {
      B[i] = static_cast<T>(100.0 * (randn() - 0.5));
      C[i] = static_cast<T>(100.0 * (randn() - 0.5));
    }
    scalar_reference::outer_product(&ARef[0], &B[0], &C[0], DIM);
  }

  void run(unsigned) override { outer_ripple(&A[0], &B[0], &C[0]); }

  bool verify() const override {
    return isclose(SZ, &A[0], &ARef[0], pow(0.1, SigDigits));
  }
};

DefineTest<OuterTest<int, 0>> TOuterInt("Outer<int>");
DefineTest<OuterTest<float, 4>> TOuterFloat("Outer<float>");

} // namespace
