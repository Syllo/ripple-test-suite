//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license
// information. SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <ripple-test-suite/ripple-test-suite.h>
#include <ripple.h>

namespace {

constexpr unsigned RippleLanes = 0;

int fir_ripple_le_32(std::size_t channel_count, std::size_t tap_count,
                     std::size_t input_frame_count, const float *input,
                     const float *coefficients, float *output) {
  ripple_block_t block = ripple_set_block_shape(RippleLanes, 32);
  const std::size_t lane = ripple_id(block, 0);

  for (std::uint32_t frame = 0; frame < input_frame_count - tap_count + 1;
       ++frame) {
    if (lane < channel_count) {
      float sum = 0.0f;
      for (std::uint32_t tap = 0; tap < tap_count; ++tap)
        sum += input[(frame + tap) * channel_count + lane] *
               coefficients[tap * channel_count + lane];
      output[frame * channel_count + lane] = sum;
    }
  }
  return 0;
}

int fir_ripple_ge_32(std::size_t channel_count, std::size_t tap_count,
                     std::size_t input_frame_count, const float *input,
                     const float *coefficients, float *output) {
  const std::uint32_t full_channel_vectors = channel_count / 32;
  const std::uint32_t remaining_channels =
      channel_count - 32 * full_channel_vectors;
  ripple_block_t block = ripple_set_block_shape(RippleLanes, 32);
  const std::size_t lane = ripple_id(block, 0);

  for (std::uint32_t frame = 0; frame < input_frame_count - tap_count + 1;
       ++frame) {
    const float *frame_input = input + frame * channel_count;
    float *frame_output = output + frame * channel_count;

    for (std::uint32_t vector = 0; vector < full_channel_vectors; ++vector) {
      const float *input0 = frame_input + 32 * vector;
      const float *coefficient0 = coefficients + 32 * vector;
      const std::uint32_t groups = tap_count / 4;
      const std::uint32_t remaining_taps = tap_count - groups * 4;
      float vector_sum = 0.0f;

      if (groups != 0) {
        const std::uint32_t group_stride = 4 * channel_count;
        const float *input1 = input0 + channel_count;
        const float *input2 = input1 + channel_count;
        const float *input3 = input2 + channel_count;
        const float *coefficient1 = coefficient0 + channel_count;
        const float *coefficient2 = coefficient1 + channel_count;
        const float *coefficient3 = coefficient2 + channel_count;
        float sum0 = 0.0f;
        float sum1 = 0.0f;
        float sum2 = 0.0f;
        float sum3 = 0.0f;
        for (std::uint32_t group = 0; group < groups; ++group) {
          sum0 += input0[lane] * coefficient0[lane];
          sum1 += input1[lane] * coefficient1[lane];
          sum2 += input2[lane] * coefficient2[lane];
          sum3 += input3[lane] * coefficient3[lane];
          input0 += group_stride;
          input1 += group_stride;
          input2 += group_stride;
          input3 += group_stride;
          coefficient0 += group_stride;
          coefficient1 += group_stride;
          coefficient2 += group_stride;
          coefficient3 += group_stride;
        }
        vector_sum = (sum0 + sum1) + (sum2 + sum3);
      }

      for (std::uint32_t tap = 0; tap < remaining_taps; ++tap) {
        vector_sum += input0[lane] * coefficient0[lane];
        input0 += channel_count;
        coefficient0 += channel_count;
      }
      frame_output[32 * vector + lane] = vector_sum;
    }
  }

  if (remaining_channels != 0) {
    const std::uint32_t channel_offset = 32 * full_channel_vectors;
    for (std::uint32_t frame = 0; frame < input_frame_count - tap_count + 1;
         ++frame) {
      const float *input0 = input + frame * channel_count + channel_offset;
      const float *coefficient0 = coefficients + channel_offset;
      const std::uint32_t groups = tap_count / 4;
      const std::uint32_t remaining_taps = tap_count - groups * 4;
      float vector_sum = 0.0f;

      if (groups != 0) {
        const std::uint32_t group_stride = 4 * channel_count;
        const float *input1 = input0 + channel_count;
        const float *input2 = input1 + channel_count;
        const float *input3 = input2 + channel_count;
        const float *coefficient1 = coefficient0 + channel_count;
        const float *coefficient2 = coefficient1 + channel_count;
        const float *coefficient3 = coefficient2 + channel_count;
        float sum0 = 0.0f;
        float sum1 = 0.0f;
        float sum2 = 0.0f;
        float sum3 = 0.0f;
        for (std::uint32_t group = 0; group < groups; ++group) {
          sum0 += input0[lane] * coefficient0[lane];
          sum1 += input1[lane] * coefficient1[lane];
          sum2 += input2[lane] * coefficient2[lane];
          sum3 += input3[lane] * coefficient3[lane];
          input0 += group_stride;
          input1 += group_stride;
          input2 += group_stride;
          input3 += group_stride;
          coefficient0 += group_stride;
          coefficient1 += group_stride;
          coefficient2 += group_stride;
          coefficient3 += group_stride;
        }
        vector_sum = (sum0 + sum1) + (sum2 + sum3);
      }

      for (std::uint32_t tap = 0; tap < remaining_taps; ++tap) {
        vector_sum += input0[lane] * coefficient0[lane];
        input0 += channel_count;
        coefficient0 += channel_count;
      }
      if (lane < remaining_channels)
        output[frame * channel_count + channel_offset + lane] = vector_sum;
    }
  }
  return 0;
}

void scalar_interleaved_fir(float *output, const float *input,
                            const float *coefficients,
                            std::size_t output_frame_count,
                            std::size_t channel_count, std::size_t tap_count) {
  for (std::size_t frame = 0; frame < output_frame_count; ++frame) {
    for (std::size_t channel = 0; channel < channel_count; ++channel) {
      float sum = 0.0f;
      for (std::size_t tap = 0; tap < tap_count; ++tap)
        sum += input[(frame + tap) * channel_count + channel] *
               coefficients[tap * channel_count + channel];
      output[frame * channel_count + channel] = sum;
    }
  }
}

using namespace ripple_test_suite;

template <std::size_t ChannelCount, std::size_t BlockLength,
          std::size_t TapCount>
class InterleavedFirTest : public Test {
  static constexpr std::size_t InputFrameCount = BlockLength + TapCount - 1;
  std::vector<float> Input, Coefficients, Output, Reference;

public:
  InterleavedFirTest(TestFramework &TestFramework)
      : Test(TestFramework), Input(ChannelCount * InputFrameCount),
        Coefficients(ChannelCount * TapCount),
        Output(ChannelCount * BlockLength, 0.0f),
        Reference(ChannelCount * BlockLength, 0.0f) {
    for (float &value : Input)
      value = static_cast<float>(randn());
    for (float &value : Coefficients)
      value = static_cast<float>(randn());
    scalar_interleaved_fir(&Reference[0], &Input[0], &Coefficients[0],
                           BlockLength, ChannelCount, TapCount);
  }

  void run(unsigned) override {
    std::fill(Output.begin(), Output.end(), 0.0f);
    if (ChannelCount < 32)
      fir_ripple_le_32(ChannelCount, TapCount, InputFrameCount, &Input[0],
                       &Coefficients[0], &Output[0]);
    else
      fir_ripple_ge_32(ChannelCount, TapCount, InputFrameCount, &Input[0],
                       &Coefficients[0], &Output[0]);
  }

  bool verify() const override {
    // The Ripple path uses four independent accumulators for full vectors,
    // while the oracle deliberately sums taps in order. Allow for the
    // resulting floating-point reassociation at the longest tap counts.
    return isclose(Output.size(), &Output[0], &Reference[0], 1e-5);
  }
};

#define FIR_TEST(ID, CHANNELS, LENGTH, TAPS)                                   \
  DefineTest<InterleavedFirTest<CHANNELS, LENGTH, TAPS>> ID(                   \
      "FIR<C=" #CHANNELS ",L=" #LENGTH ",T=" #TAPS ">")

#define FIR_LENGTHS(PREFIX, CHANNELS, TAPS)                                    \
  FIR_TEST(PREFIX##30, CHANNELS, 30, TAPS);                                    \
  FIR_TEST(PREFIX##48, CHANNELS, 48, TAPS);                                    \
  FIR_TEST(PREFIX##64, CHANNELS, 64, TAPS)

#define FIR_SMALL_CHANNEL_SET(PREFIX, CHANNELS)                                \
  FIR_LENGTHS(PREFIX##T16, CHANNELS, 16);                                      \
  FIR_LENGTHS(PREFIX##T17, CHANNELS, 17);                                      \
  FIR_LENGTHS(PREFIX##T31, CHANNELS, 31);                                      \
  FIR_LENGTHS(PREFIX##T32, CHANNELS, 32);                                      \
  FIR_LENGTHS(PREFIX##T33, CHANNELS, 33);                                      \
  FIR_LENGTHS(PREFIX##T256, CHANNELS, 256);                                    \
  FIR_LENGTHS(PREFIX##T1024, CHANNELS, 1024)

FIR_SMALL_CHANNEL_SET(FIRC3, 3);
FIR_SMALL_CHANNEL_SET(FIRC8, 8);
FIR_SMALL_CHANNEL_SET(FIRC9, 9);
FIR_SMALL_CHANNEL_SET(FIRC16, 16);

#define FIR_LARGE_CHANNEL_SET(PREFIX, CHANNELS)                                \
  FIR_TEST(PREFIX##L64T32, CHANNELS, 64, 32);                                  \
  FIR_TEST(PREFIX##L128T32, CHANNELS, 128, 32);                                \
  FIR_TEST(PREFIX##L64T256, CHANNELS, 64, 256);                                \
  FIR_TEST(PREFIX##L128T256, CHANNELS, 128, 256)

FIR_LARGE_CHANNEL_SET(FIRC2, 2);
FIR_LARGE_CHANNEL_SET(FIRC7, 7);
FIR_LARGE_CHANNEL_SET(FIRC32, 32);
FIR_LARGE_CHANNEL_SET(FIRC33, 33);
FIR_LARGE_CHANNEL_SET(FIRC64, 64);

#undef FIR_LARGE_CHANNEL_SET
#undef FIR_SMALL_CHANNEL_SET
#undef FIR_LENGTHS
#undef FIR_TEST

} // namespace
