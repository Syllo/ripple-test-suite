//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license
// information. SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <ripple-test-suite/ripple-test-suite.h>
#include <ripple.h>

namespace {

struct hxv_biquad_t {
  const float *coeffs;
  float *states;
  size_t nChannels;
  size_t nTotalVect;
  size_t nFullVpS;
  size_t nStages;
  size_t nSlicePV;
  size_t wSliceCh;
};

enum FilterVariant { DF1, DF2, TDF2 };

template <FilterVariant FT>
int32_t biquad_ripple(hxv_biquad_t *bq, const float *src, uint32_t src_inc,
                          float *dst, uint32_t dst_inc, uint32_t blockSize) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);

  // we have an array of [coefs][stage][channel]
  size_t coef_stride = bq->nTotalVect * 32;
  // nFullVpS counts 32-float vectors; pointer arithmetic counts floats.
  size_t stage_stride = bq->nFullVpS * 32;
  ripple_parallel_full(BS, 0);
  for (size_t ch = 0; ch < bq->nChannels; ++ch) {
    const float *sample_src = src;
    for (size_t stage = 0; stage < bq->nStages; ++stage) {
      float b0 = bq->coeffs[stage * stage_stride + ch];
      float b1 = bq->coeffs[coef_stride + stage * stage_stride + ch];
      float b2 = bq->coeffs[2 * coef_stride + stage * stage_stride + ch];
      float a1 = bq->coeffs[3 * coef_stride + stage * stage_stride + ch];
      float a2 = bq->coeffs[4 * coef_stride + stage * stage_stride + ch];
      float s1 = bq->states[stage * stage_stride + ch];
      float s2 = bq->states[coef_stride + stage * stage_stride + ch];
      float s3, s4;
      if (FT == DF1) {
        s3 = bq->states[2 * coef_stride + stage * stage_stride + ch];
        s4 = bq->states[3 * coef_stride + stage * stage_stride + ch];
      } else if (FT == DF2 || FT == TDF2) {
        // do nothing.
      } else {
        assert(false); // didn't implement other variants.
      }

      for (size_t i = 0; i < blockSize; ++i) {
        float x = sample_src[src_inc * i + ch];
        float y;
        if (FT == DF1) {
          y = b0 * x + b1 * s1 + b2 * s2 - a1 * s3 - a2 * s4;
          // rotate states
          s2 = s1; // x(n-2) <- x(n-1)
          s1 = x;  // x(n-1) <- x(n)
          s4 = s3; // y(n-2) <- y(n-1)
          s3 = y;  // y(n-1) <- y(n)
        } else if (FT == DF2 || FT == TDF2) {
          // TODO: Not sure if this inherited code is correct.
          y = b0 * x + b1 * s1 - a1 * s2;
          s1 = x; // x(n-1) <- x(n)
          s2 = y; // y(n-1) <- y(n)
        } else {
          assert(false);
        }
        dst[dst_inc * i + ch] = y;
      }
      // store states
      bq->states[stage * stage_stride + ch] = s1;
      bq->states[coef_stride + stage * stage_stride + ch] = s2;
      if (FT == DF1) {
        bq->states[2 * coef_stride + stage * stage_stride + ch] = s3;
        bq->states[3 * coef_stride + stage * stage_stride + ch] = s4;
      } else if (FT == DF2 || FT == TDF2) {
        // do nothing.
      } else {
        assert(false);
      }
      // After the 1st stage, use the output as input
      sample_src = dst;
    }
  }
  return 0;
}

void scalar_biquad_df1(float *output, float *states, const float *input,
                       const float *coefficients, std::size_t frame_count,
                       std::size_t channel_count, std::size_t stage_count) {
  const std::size_t coefficient_plane_stride = stage_count * channel_count;

  // Each stage owns one row of channel state. Load the row before processing
  // the stage, then commit it as one block.
  for (std::size_t stage = 0; stage < stage_count; ++stage) {
    std::vector<float> previous_input(channel_count);
    std::vector<float> second_previous_input(channel_count);
    std::vector<float> previous_output(channel_count);
    std::vector<float> second_previous_output(channel_count);

    for (std::size_t channel = 0; channel < channel_count; ++channel) {
      const std::size_t packed_lane = stage * channel_count + channel;
      previous_input[channel] = states[packed_lane];
      second_previous_input[channel] =
          states[coefficient_plane_stride + packed_lane];
      previous_output[channel] =
          states[2 * coefficient_plane_stride + packed_lane];
      second_previous_output[channel] =
          states[3 * coefficient_plane_stride + packed_lane];
    }

    for (std::size_t frame = 0; frame < frame_count; ++frame) {
      for (std::size_t channel = 0; channel < channel_count; ++channel) {
        const std::size_t packed_lane = stage * channel_count + channel;
        const std::size_t sample = frame * channel_count + channel;
        const float current_input = stage == 0 ? input[sample] : output[sample];
        const float b0 = coefficients[packed_lane];
        const float b1 = coefficients[coefficient_plane_stride + packed_lane];
        const float b2 =
            coefficients[2 * coefficient_plane_stride + packed_lane];
        const float a1 =
            coefficients[3 * coefficient_plane_stride + packed_lane];
        const float a2 =
            coefficients[4 * coefficient_plane_stride + packed_lane];

        const float current_output = b0 * current_input +
                                     b1 * previous_input[channel] +
                                     b2 * second_previous_input[channel] -
                                     a1 * previous_output[channel] -
                                     a2 * second_previous_output[channel];
        output[sample] = current_output;

        second_previous_input[channel] = previous_input[channel];
        previous_input[channel] = current_input;
        second_previous_output[channel] = previous_output[channel];
        previous_output[channel] = current_output;
      }
    }

    for (std::size_t channel = 0; channel < channel_count; ++channel) {
      const std::size_t packed_lane = stage * channel_count + channel;
      states[packed_lane] = previous_input[channel];
      states[coefficient_plane_stride + packed_lane] =
          second_previous_input[channel];
      states[2 * coefficient_plane_stride + packed_lane] =
          previous_output[channel];
      states[3 * coefficient_plane_stride + packed_lane] =
          second_previous_output[channel];
    }
  }
}

using namespace ripple_test_suite;

template <unsigned Nstage, unsigned Nchannel> class BiquadTest : public Test {
  static constexpr uint32_t BlockSize = 160;
  static constexpr uint32_t StateCount = 4;
  static constexpr std::size_t SampleCount = BlockSize * Nchannel;
  static constexpr std::size_t CoefficientCount = 5 * Nstage * Nchannel;
  static constexpr std::size_t StoredStateCount =
      StateCount * Nstage * Nchannel;

  std::vector<float> Input, Output, Reference, Coefficients, States,
      ReferenceStates;

public:
  BiquadTest(TestFramework &TestFramework)
      : Test(TestFramework), Input(SampleCount), Output(SampleCount, 0.0f),
        Reference(SampleCount, 0.0f), Coefficients(CoefficientCount, 0.0f),
        States(StoredStateCount, 0.0f),
        ReferenceStates(StoredStateCount, 0.0f) {
    for (float &sample : Input)
      sample = static_cast<float>(randn());

    static constexpr float CoefficientValues[5] = {0.29292f, 0.5858f, 0.2929f,
                                                   0.0f, 0.17161f};
    for (std::size_t coefficient = 0; coefficient < 5; ++coefficient)
      for (std::size_t stage = 0; stage < Nstage; ++stage)
        for (std::size_t channel = 0; channel < Nchannel; ++channel)
          Coefficients[coefficient * Nstage * Nchannel + stage * Nchannel +
                       channel] = CoefficientValues[coefficient];

    scalar_biquad_df1(&Reference[0], &ReferenceStates[0], &Input[0],
                      &Coefficients[0], BlockSize, Nchannel, Nstage);
  }

  void run(unsigned) override {
    std::fill(Output.begin(), Output.end(), 0.0f);
    std::fill(States.begin(), States.end(), 0.0f);
    hxv_biquad_t biquad = {
        &Coefficients[0], &States[0], Nchannel, Nstage * Nchannel / 32,
        Nchannel / 32,    Nstage,     0,        0};
    biquad_ripple<DF1>(&biquad, &Input[0], Nchannel, &Output[0], Nchannel,
                           BlockSize);
  }

  bool verify() const override {
    return isclose(SampleCount, &Output[0], &Reference[0]) &&
           isclose(StoredStateCount, &States[0], &ReferenceStates[0]);
  }
};

DefineTest<BiquadTest<4, 32>> TBiquadDF1("Biquad.DF1<NStg=4,NCh=32>");

} // namespace
