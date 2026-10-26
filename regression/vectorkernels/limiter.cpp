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
void limiter_ripple(T *__restrict__ pA, T *__restrict__ pB,
                    T *__restrict__ pC) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  ripple_parallel(BS, 0);
  for (int i = 0; i < N; i++) {
    T l = pB[i];
    T r = pC[i];
    T absl = l < 0 ? -l : l;
    T absr = r < 0 ? -r : r;
    T peak = absl > absr ? absl : absr;
    T threshold = 4;
    pA[i] = peak > threshold ? threshold : l;
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class LimiterTest : public Test {
  std::vector<T> A, B, C, ARef;

public:
  LimiterTest(TestFramework &TF)
      : Test(TF), A(N, 0), B(N, 0), C(N, 0), ARef(N, 0) {
    for (int i = 0; i < N; i++) {
      B[i] = static_cast<T>(20.0 * (randn() - 0.5));
      C[i] = static_cast<T>(20.0 * (randn() - 0.5));
    }
    scalar_reference::transform(&ARef[0], &B[0], &C[0], N, [](T lhs, T rhs) {
      const T left_magnitude = lhs < T{} ? -lhs : lhs;
      const T right_magnitude = rhs < T{} ? -rhs : rhs;
      const T peak =
          left_magnitude > right_magnitude ? left_magnitude : right_magnitude;
      const T threshold = static_cast<T>(4);
      return peak > threshold ? threshold : lhs;
    });
  }

  void run(unsigned) override { limiter_ripple(&A[0], &B[0], &C[0]); }

  bool verify() const override {
    return isclose(N, &A[0], &ARef[0], pow(0.1, SigDigits));
  }
};

DefineTest<LimiterTest<int, 0>> TLimiterInt("Limiter<int>");
DefineTest<LimiterTest<float, 4>> TLimiterFloat("Limiter<float>");

} // namespace
