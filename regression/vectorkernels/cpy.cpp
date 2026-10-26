//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <cstddef>

#include "scalar_reference.h"
#include <ripple-test-suite/ripple-test-suite.h>
#include <ripple.h>

namespace {

static constexpr int N = 400;

template <typename T>
void cpy_ripple(T *__restrict__ A, const T *__restrict__ B) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  ripple_parallel(BS, 0);
  for (int i = 0; i < N; i++) {
    A[i] = B[i];
  }
}

using namespace ripple_test_suite;

template <typename T> class CpyTest : public Test {
  std::vector<T> A, B, ARef;

public:
  CpyTest(TestFramework &TF) : Test(TF), A(N, 0), B(N, 0), ARef(N, 0) {
    for (int i = 0; i < N; i++)
      B[i] = static_cast<T>(100 * (randn() - 0.5));
    scalar_reference::transform(&ARef[0], &B[0], N,
                                [](T value) { return value; });
  }

  void run(unsigned) override { cpy_ripple(&A[0], &B[0]); }

  bool verify() const override { return isclose(N, &A[0], &ARef[0], 1e-5); }
};

DefineTest<CpyTest<int>> TCpyInt("Cpy<int>");
DefineTest<CpyTest<float>> TCpyFloat("Cpy<float>");

} // namespace
