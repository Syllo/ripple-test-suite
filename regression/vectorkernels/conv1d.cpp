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

namespace {

static constexpr int InputSize = 400;
static constexpr int KernelSize = 5;
static constexpr int OutputSize = InputSize - KernelSize + 1;

template <typename T>
void conv1d_ripple(T *__restrict__ output, T *__restrict__ input,
                   T *__restrict__ kernel) {
  ripple_block_t BS = ripple_set_block_shape(0, KernelSize, 6);
  ripple_parallel(BS, 1);
  for (int i = 0; i < OutputSize; ++i) {
    T sum = T{};
    ripple_parallel(BS, 0);
    for (int j = 0; j < KernelSize; ++j)
      sum += input[i + j] * kernel[j];
    output[i] = ripple_reduceadd(0b1, sum);
  }
}

template <typename T>
void scalar_valid_convolution(T *output, const T *input, const T *kernel) {
  for (int output_index = 0; output_index < OutputSize; ++output_index) {
    T accumulated = T{};
    for (int kernel_index = 0; kernel_index < KernelSize; ++kernel_index) {
      const int input_index = output_index + kernel_index;
      accumulated += input[input_index] * kernel[kernel_index];
    }
    output[output_index] = accumulated;
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class Conv1DTest : public Test {
  std::vector<T> Output, Input, Kernel, Reference;

public:
  Conv1DTest(TestFramework &TF)
      : Test(TF), Output(OutputSize, 0), Input(InputSize, 0),
        Kernel(KernelSize, 0), Reference(OutputSize, 0) {
    for (int i = 0; i < InputSize; ++i)
      Input[i] = static_cast<T>(100.0 * (randn() - 0.5));
    for (int i = 0; i < KernelSize; ++i)
      Kernel[i] = static_cast<T>(2.0 * (randn() - 0.5));
    scalar_valid_convolution(&Reference[0], &Input[0], &Kernel[0]);
  }

#ifdef __hexagon__
  // The upstream Hexagon backend miscompiles this padded floating-point
  // reduction. The downstream Hexagon compiler already contains a fix.
  std::pair<bool, std::string> xfail() const override {
    return {SigDigits == 4,
            "Upstream Hexagon backend miscompiles the padded float reduction."};
  }
#endif

  void run(unsigned) override {
    conv1d_ripple(&Output[0], &Input[0], &Kernel[0]);
  }

  bool verify() const override {
    return isclose(OutputSize, &Output[0], &Reference[0],
                   pow(0.1, SigDigits));
  }
};

DefineTest<Conv1DTest<int, 0>> TConv1DInt("Conv1D<int>");
DefineTest<Conv1DTest<float, 4>> TConv1DFloat("Conv1D<float>");

} // namespace
