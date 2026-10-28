//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license
// information. SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <ripple-test-suite/ripple-test-suite.h>
#include <ripple.h>

namespace {

static inline int max_val(int a, int b) { return a > b ? a : b; }

template <typename T>
int conv_ripple(T *out_ptr, const T *in_ptr, const T *filt_ptr,
                const T *scale_ptr, size_t h_stride, size_t w_stride,
                size_t filt_h, size_t filt_w, size_t h_in, size_t w_in,
                size_t d_in, size_t b_out, size_t h_out, size_t w_out,
                size_t d_out) {
  size_t h_adj =
      max_val((int)((h_out - 1) * h_stride + filt_h) - (int)h_in, 0) / 2;
  size_t w_adj =
      max_val((int)((w_out - 1) * w_stride + filt_w) - (int)w_in, 0) / 2;

  int in_s2 = d_in, in_s1 = w_in * in_s2, in_s0 = h_in * in_s1;
  int out_s2 = d_out, out_s1 = w_out * out_s2, out_s0 = h_out * out_s1;
  int filt_s2 = d_out, filt_s1 = d_in * filt_s2, filt_s0 = filt_w * filt_s1;

  ripple_block_t BS = ripple_set_block_shape(0, 3);
  ripple_parallel(BS, 0);
  for (int b = 0; b < (int)b_out; b++) {
    for (int h = 0; h < (int)h_out; h++) {
      for (int w = 0; w < (int)w_out; w++) {
        for (int d = 0; d < (int)d_out; d++) {
          float val = scale_ptr[d];
          float scaleval = (val <= 0.0f) ? 1.0f : val;
          float sum = 0;
          for (int hh = 0; hh < (int)filt_h; hh++) {
            int in_h = (h * h_stride) + hh - h_adj;
            if (in_h < 0 || in_h >= (int)h_in)
              continue;
            for (int ww = 0; ww < (int)filt_w; ww++) {
              int in_w = (w * w_stride) + ww - w_adj;
              if (in_w < 0 || in_w >= (int)w_in)
                continue;
              for (int dd = 0; dd < (int)d_in; dd++) {
                int in_idx = b * in_s0 + in_h * in_s1 + in_w * in_s2 + dd;
                int filt_idx = hh * filt_s0 + ww * filt_s1 + dd * filt_s2 + d;
                sum += (float)in_ptr[in_idx] * (float)filt_ptr[filt_idx];
              }
            }
          }
          int out_idx = b * out_s0 + h * out_s1 + w * out_s2 + d;
          out_ptr[out_idx] = (T)(sum * scaleval);
        }
      }
    }
  }
  return 0;
}

template <typename T>
void scalar_nhwc_convolution(T *output, const T *input, const T *filter,
                             const T *scale, std::size_t vertical_stride,
                             std::size_t horizontal_stride,
                             std::size_t filter_height,
                             std::size_t filter_width, std::size_t input_height,
                             std::size_t input_width,
                             std::size_t input_channels,
                             std::size_t batch_count, std::size_t output_height,
                             std::size_t output_width,
                             std::size_t output_channels) {
  const int needed_height =
      static_cast<int>((output_height - 1) * vertical_stride + filter_height);
  const int needed_width =
      static_cast<int>((output_width - 1) * horizontal_stride + filter_width);
  const int top_padding =
      std::max(needed_height - static_cast<int>(input_height), 0) / 2;
  const int left_padding =
      std::max(needed_width - static_cast<int>(input_width), 0) / 2;

  for (std::size_t batch = 0; batch < batch_count; ++batch) {
    for (std::size_t output_row = 0; output_row < output_height; ++output_row) {
      for (std::size_t output_column = 0; output_column < output_width;
           ++output_column) {
        for (std::size_t output_channel = 0; output_channel < output_channels;
             ++output_channel) {
          float accumulated = 0.0f;

          for (std::size_t filter_row = 0; filter_row < filter_height;
               ++filter_row) {
            const int input_row =
                static_cast<int>(output_row * vertical_stride + filter_row) -
                top_padding;
            if (input_row < 0 || input_row >= static_cast<int>(input_height))
              continue;

            for (std::size_t filter_column = 0; filter_column < filter_width;
                 ++filter_column) {
              const int input_column =
                  static_cast<int>(output_column * horizontal_stride +
                                   filter_column) -
                  left_padding;
              if (input_column < 0 ||
                  input_column >= static_cast<int>(input_width))
                continue;

              for (std::size_t input_channel = 0;
                   input_channel < input_channels; ++input_channel) {
                const std::size_t input_offset =
                    ((batch * input_height + input_row) * input_width +
                     input_column) *
                        input_channels +
                    input_channel;
                const std::size_t filter_offset =
                    ((filter_row * filter_width + filter_column) *
                         input_channels +
                     input_channel) *
                        output_channels +
                    output_channel;
                accumulated += static_cast<float>(input[input_offset]) *
                               static_cast<float>(filter[filter_offset]);
              }
            }
          }

          const float requested_scale =
              static_cast<float>(scale[output_channel]);
          const float applied_scale =
              requested_scale > 0.0f ? requested_scale : 1.0f;
          const std::size_t output_offset =
              ((batch * output_height + output_row) * output_width +
               output_column) *
                  output_channels +
              output_channel;
          output[output_offset] = static_cast<T>(accumulated * applied_scale);
        }
      }
    }
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class ConvTest : public Test {
  // b=1, h_in=5, w_in=5, d_in=4, filt_h=3, filt_w=3, h_out=3, w_out=3, d_out=4
  static constexpr size_t B_OUT = 1, H_IN = 5, W_IN = 5, D_IN = 4;
  static constexpr size_t FILT_H = 3, FILT_W = 3;
  static constexpr size_t H_OUT = 3, W_OUT = 3, D_OUT = 4;
  static constexpr size_t H_STRIDE = 1, W_STRIDE = 1;

  std::vector<T> In, Filt, Scale, Out, OutRef;

public:
  ConvTest(TestFramework &TF)
      : Test(TF), In(B_OUT * H_IN * W_IN * D_IN, 0),
        Filt(FILT_H * FILT_W * D_IN * D_OUT, 0), Scale(D_OUT, 0),
        Out(B_OUT * H_OUT * W_OUT * D_OUT, 0),
        OutRef(B_OUT * H_OUT * W_OUT * D_OUT, 0) {
    for (auto &v : In)
      v = static_cast<T>(10.0 * (randn() - 0.5));
    for (auto &v : Filt)
      v = static_cast<T>(2.0 * (randn() - 0.5));
    for (auto &v : Scale)
      v = static_cast<T>(0.5 + randn());
    scalar_nhwc_convolution(&OutRef[0], &In[0], &Filt[0], &Scale[0], H_STRIDE,
                            W_STRIDE, FILT_H, FILT_W, H_IN, W_IN, D_IN, B_OUT,
                            H_OUT, W_OUT, D_OUT);
  }

  void run(unsigned) override {
    conv_ripple(&Out[0], &In[0], &Filt[0], &Scale[0], H_STRIDE, W_STRIDE,
                FILT_H, FILT_W, H_IN, W_IN, D_IN, B_OUT, H_OUT, W_OUT, D_OUT);
  }

  bool verify() const override {
    return isclose(B_OUT * H_OUT * W_OUT * D_OUT, &Out[0], &OutRef[0],
                   pow(0.1, SigDigits));
  }
};

DefineTest<ConvTest<float, 4>> TConvFloat("Conv<float>");

} // namespace
