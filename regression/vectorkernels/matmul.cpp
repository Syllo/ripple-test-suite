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
void matmul_ripple(T *__restrict__ pA, const T *__restrict__ pB,
                   const T *__restrict__ pC) {
  ripple_block_t BS = ripple_set_block_shape(0, DIM);
  ripple_parallel(BS, 0);
  for (int i = 0; i < DIM; i++) {
    for (int j = 0; j < DIM; j++) {
      T sum = (T)0;
      for (int k = 0; k < DIM; k++) {
        sum += pB[i * DIM + k] * pC[k * DIM + j];
      }
      pA[i * DIM + j] = sum;
    }
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class MatmulTest : public Test {
  static constexpr int SZ = DIM * DIM;
  std::vector<T> A, B, C, ARef;

public:
  MatmulTest(TestFramework &TF)
      : Test(TF), A(SZ, 0), B(SZ, 0), C(SZ, 0), ARef(SZ, 0) {
    for (int i = 0; i < SZ; i++) {
      B[i] = static_cast<T>(10.0 * (randn()));
      C[i] = static_cast<T>(10.0 * (randn()));
    }
    scalar_reference::matrix_multiply(&ARef[0], &B[0], &C[0], DIM);
  }

  void run(unsigned) override { matmul_ripple(&A[0], &B[0], &C[0]); }

  bool verify() const override {
    return isclose(SZ, &A[0], &ARef[0], pow(0.1, SigDigits));
  }
};

DefineTest<MatmulTest<int, 0>> TMatmulInt("Matmul<int>");
DefineTest<MatmulTest<float, 3>> TMatmulFloat("Matmul<float>");

} // namespace
