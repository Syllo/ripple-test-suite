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

template <typename T> T red_ripple(T *__restrict__ pA) {
  constexpr int CHUNK = 32;
  constexpr int REM = N % CHUNK;
  T sum = 0;
  ripple_block_t BS = ripple_set_block_shape(0, CHUNK);
  T partial = 0;

  for (int base = 0; base < (N - REM); base += CHUNK) {
    partial = partial + pA[base + ripple_id(BS, 0)];
  }

  sum = ripple_reduceadd(0x1, partial);

  if (REM > 0) {
    ripple_block_t BS_rem = ripple_set_block_shape(0, REM);
    T partial_rem = pA[N - REM + ripple_id(BS_rem, 0)];
    sum = sum + ripple_reduceadd(0x1, partial_rem);
  }
  return sum;
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class RedTest : public Test {
  std::vector<T> A;
  T Result, ResultRef;

public:
  RedTest(TestFramework &TF) : Test(TF), A(N, 0), Result(0), ResultRef(0) {
    for (int i = 0; i < N; i++)
      A[i] = static_cast<T>(10.0 * (randn() - 0.5));
    ResultRef = scalar_reference::sum(&A[0], N);
  }

  void run(unsigned) override { Result = red_ripple(&A[0]); }

  bool verify() const override {
    return isclose(1, &Result, &ResultRef, pow(0.1, SigDigits));
  }
};

DefineTest<RedTest<int, 0>> TRedInt("Red<int>");
DefineTest<RedTest<float, 2>> TRedFloat("Red<float>");

} // namespace
