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
static constexpr int NUM_ELEMS = N / 2 - 1;

template <typename T>
void strided_load_ripple(T *__restrict__ pA, T *__restrict__ pB,
                         T *__restrict__ pC) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  ripple_parallel(BS, 0);
  for (int i = 0; i < NUM_ELEMS; i++) {
    pB[i] = pA[2 * i];
    pC[i] = pA[2 * i + 1];
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class StridedLoadTest : public Test {
  std::vector<T> A, B, C, BRef, CRef;

public:
  StridedLoadTest(TestFramework &TF)
      : Test(TF), A(N, 0), B(NUM_ELEMS, 0), C(NUM_ELEMS, 0), BRef(NUM_ELEMS, 0),
        CRef(NUM_ELEMS, 0) {
    for (int i = 0; i < N; i++)
      A[i] = static_cast<T>(100.0 * (randn() - 0.5));
    scalar_reference::split_pairs(&A[0], &BRef[0], &CRef[0], NUM_ELEMS);
  }

  void run(unsigned) override { strided_load_ripple(&A[0], &B[0], &C[0]); }

  bool verify() const override {
    return isclose(NUM_ELEMS, &B[0], &BRef[0], pow(0.1, SigDigits)) &&
           isclose(NUM_ELEMS, &C[0], &CRef[0], pow(0.1, SigDigits));
  }
};

DefineTest<StridedLoadTest<int, 0>> TStridedLoadInt("StridedLoad<int>");
DefineTest<StridedLoadTest<float, 4>> TStridedLoadFloat("StridedLoad<float>");

} // namespace
