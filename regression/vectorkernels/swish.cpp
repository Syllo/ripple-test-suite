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

#include <ripple-test-suite/ripple-test-suite.h>
#include <ripple.h>
#include <ripple_math.h>

namespace {

float swish_logic(float value) {
  return value / (1.0f + exp(-value));
}

int swish_logic(int value) {
  const float input = static_cast<float>(value);
  const float activated = input / (1.0f + exp(-input));
  return static_cast<int>(round(activated));
}

template <typename T>
void __attribute__((always_inline)) swish_ripple(T *output, const T *input,
                                                 std::size_t count) {
  ripple_block_t BS = ripple_set_block_shape(0, 256);
  ripple_parallel(BS, 0);
  for (std::size_t i = 0; i < count; ++i)
    output[i] = swish_logic(input[i]);
}

#undef exp
#undef round

template <typename T> struct ScalarSwish {
  static T evaluate(T value) {
    const float input = static_cast<float>(value);
    return static_cast<T>(input / (1.0f + std::exp(-input)));
  }
};

template <> struct ScalarSwish<int> {
  static int evaluate(int value) {
    const float input = static_cast<float>(value);
    const float activated = input / (1.0f + std::exp(-input));
    return static_cast<int>(std::round(activated));
  }
};

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class SwishTest : public Test {
  static constexpr std::size_t Count = 256;
  std::vector<T> Input, Output, Reference;

public:
  SwishTest(TestFramework &TF)
      : Test(TF), Input(Count, 0), Output(Count, 0), Reference(Count, 0) {
    for (std::size_t i = 0; i < Count; ++i) {
      Input[i] = static_cast<T>(10.0 * (randn() - 0.5));
      Reference[i] = ScalarSwish<T>::evaluate(Input[i]);
    }
  }

  void run(unsigned) override { swish_ripple(&Output[0], &Input[0], Count); }

  bool verify() const override {
    return isclose(Count, &Output[0], &Reference[0], pow(0.1, SigDigits));
  }
};

DefineTest<SwishTest<int, 0>> TSwishInt("Swish<int>");
DefineTest<SwishTest<float, 4>> TSwishFloat("Swish<float>");

} // namespace
