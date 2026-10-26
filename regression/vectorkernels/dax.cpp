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
void dax_ripple(const T *__restrict__ pA, const T *__restrict__ pB,
                T *__restrict__ pC) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  ripple_parallel(BS, 0);
  for (int i = 0; i < N; i++) {
    pC[i] = pA[i] + (T)7 * pB[i];
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class DaxTest : public Test {
  std::vector<T> A, B, C, CRef;

public:
  DaxTest(TestFramework &TF) : Test(TF), A(N, 0), B(N, 0), C(N, 0), CRef(N, 0) {
    for (int i = 0; i < N; i++) {
      A[i] = static_cast<T>(100.0 * (randn() - 0.5));
      B[i] = static_cast<T>(100.0 * (randn() - 0.5));
    }
    scalar_reference::transform(&CRef[0], &A[0], &B[0], N, [](T lhs, T rhs) {
      return lhs + static_cast<T>(7) * rhs;
    });
  }

  void run(unsigned) override { dax_ripple(&A[0], &B[0], &C[0]); }

  bool verify() const override {
    return isclose(N, &C[0], &CRef[0], pow(0.1, SigDigits));
  }
};

DefineTest<DaxTest<int, 0>> TDaxInt("Dax<int>");
DefineTest<DaxTest<float, 4>> TDaxFloat("Dax<float>");

} // namespace
