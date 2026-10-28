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
int32_t hvx_biquad_ripple(hxv_biquad_t *bq, const float *src, uint32_t src_inc,
                          float *dst, uint32_t dst_inc, uint32_t blockSize) {
  ripple_block_t BS = ripple_set_block_shape(0, 32);
  size_t v0 = ripple_id(BS, 0);

  // we have an array of [coefs][stage][channel]
  size_t coef_stride = bq->nTotalVect * 32;
  // nFullVpS counts 32-float vectors; pointer arithmetic counts floats.
  size_t stage_stride = bq->nFullVpS * 32;
  // Note: we assume bq->nChannels is divisible by 32.
  for (size_t ch_out = 0; ch_out < bq->nChannels; ch_out += 32) {
    size_t ch = ch_out + v0;
    const float *sample_src = src;
    for (size_t stage = 0; stage < bq->nStages; ++stage) {
      const float *b0ptr = &bq->coeffs[stage * stage_stride + ch_out];
      const float *b1ptr =
          &bq->coeffs[coef_stride + stage * stage_stride + ch_out];
      const float *b2ptr =
          &bq->coeffs[2 * coef_stride + stage * stage_stride + ch_out];
      const float *a1ptr =
          &bq->coeffs[3 * coef_stride + stage * stage_stride + ch_out];
      const float *a2ptr =
          &bq->coeffs[4 * coef_stride + stage * stage_stride + ch_out];
      float *s1ptr = &bq->states[stage * stage_stride + ch_out];
      float *s2ptr = &bq->states[coef_stride + stage * stage_stride + ch_out];
      float *s3ptr;
      float *s4ptr;
      b0ptr = ripple_ptr_alignment(b0ptr, 128);
      b1ptr = ripple_ptr_alignment(b1ptr, 128);
      b2ptr = ripple_ptr_alignment(b2ptr, 128);
      a1ptr = ripple_ptr_alignment(a1ptr, 128);
      a2ptr = ripple_ptr_alignment(a2ptr, 128);
      s1ptr = ripple_ptr_alignment(s1ptr, 128);
      s2ptr = ripple_ptr_alignment(s2ptr, 128);

      float b0 = b0ptr[v0];
      float b1 = b1ptr[v0];
      float b2 = b2ptr[v0];
      float a1 = a1ptr[v0];
      float a2 = a2ptr[v0];
      float s1 = s1ptr[v0];
      float s2 = s2ptr[v0];
      float s3, s4;
      if (FT == DF1) {
        s3ptr = &bq->states[2 * coef_stride + stage * stage_stride + ch_out];
        s4ptr = &bq->states[3 * coef_stride + stage * stage_stride + ch_out];
        s3ptr = ripple_ptr_alignment(s3ptr, 128);
        s4ptr = ripple_ptr_alignment(s4ptr, 128);

        s3 = s3ptr[v0];
        s4 = s4ptr[v0];
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
      s1ptr[v0] = s1;
      s2ptr[v0] = s2;
      if (FT == DF1) {
        s3ptr = ripple_ptr_alignment(s3ptr, 128);
        s4ptr = ripple_ptr_alignment(s4ptr, 128);
        s3ptr[v0] = s3;
        s4ptr[v0] = s4;
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
  const std::size_t plane_stride = stage_count * channel_count;
  float previous_input[32];
  float second_previous_input[32];
  float previous_output[32];
  float second_previous_output[32];

  assert(channel_count == 32);
  for (std::size_t stage = 0; stage < stage_count; ++stage) {
    for (std::size_t channel = 0; channel < channel_count; ++channel) {
      const std::size_t lane = stage * channel_count + channel;
      previous_input[channel] = states[lane];
      second_previous_input[channel] = states[plane_stride + lane];
      previous_output[channel] = states[2 * plane_stride + lane];
      second_previous_output[channel] = states[3 * plane_stride + lane];
    }

    for (std::size_t frame = 0; frame < frame_count; ++frame) {
      for (std::size_t channel = 0; channel < channel_count; ++channel) {
        const std::size_t lane = stage * channel_count + channel;
        const std::size_t sample = frame * channel_count + channel;
        const float x = stage == 0 ? input[sample] : output[sample];
        const float y =
            coefficients[lane] * x +
            coefficients[plane_stride + lane] * previous_input[channel] +
            coefficients[2 * plane_stride + lane] *
                second_previous_input[channel] -
            coefficients[3 * plane_stride + lane] * previous_output[channel] -
            coefficients[4 * plane_stride + lane] *
                second_previous_output[channel];
        output[sample] = y;
        second_previous_input[channel] = previous_input[channel];
        previous_input[channel] = x;
        second_previous_output[channel] = previous_output[channel];
        previous_output[channel] = y;
      }
    }

    for (std::size_t channel = 0; channel < channel_count; ++channel) {
      const std::size_t lane = stage * channel_count + channel;
      states[lane] = previous_input[channel];
      states[plane_stride + lane] = second_previous_input[channel];
      states[2 * plane_stride + lane] = previous_output[channel];
      states[3 * plane_stride + lane] = second_previous_output[channel];
    }
  }
}

using namespace ripple_test_suite;

#ifdef __hexagon__
template <unsigned Nstage, unsigned Nchannel>
class BiquadScratchpadTest : public VTCMTest {
  static constexpr uint32_t BlockSize = 160;
  static constexpr uint32_t StateCount = 4;
  static constexpr std::size_t SampleCount = BlockSize * Nchannel;
  static constexpr std::size_t CoefficientCount = 5 * Nstage * Nchannel;
  static constexpr std::size_t StoredStateCount =
      StateCount * Nstage * Nchannel;

  float Input[SampleCount];
  float Output[SampleCount];
  float Reference[SampleCount];
  float ReferenceStates[StoredStateCount];
  float *Coefficients;
  float *States;

public:
  BiquadScratchpadTest(TestFramework &TestFramework)
      : VTCMTest(TestFramework),
        Coefficients(alloc<float>(CoefficientCount, 128)),
        States(alloc<float>(StoredStateCount, 128)) {
    std::fill(Output, Output + SampleCount, 0.0f);
    std::fill(Reference, Reference + SampleCount, 0.0f);
    std::fill(States, States + StoredStateCount, 0.0f);
    std::fill(ReferenceStates, ReferenceStates + StoredStateCount, 0.0f);
    for (float &sample : Input)
      sample = static_cast<float>(randn());

    static constexpr float CoefficientValues[5] = {0.29292f, 0.5858f, 0.2929f,
                                                   0.0f, 0.17161f};
    for (std::size_t coefficient = 0; coefficient < 5; ++coefficient)
      for (std::size_t stage = 0; stage < Nstage; ++stage)
        for (std::size_t channel = 0; channel < Nchannel; ++channel)
          Coefficients[coefficient * Nstage * Nchannel + stage * Nchannel +
                       channel] = CoefficientValues[coefficient];

    scalar_biquad_df1(Reference, ReferenceStates, Input, Coefficients,
                      BlockSize, Nchannel, Nstage);
  }

  void run(unsigned) override {
    std::fill(Output, Output + SampleCount, 0.0f);
    std::fill(States, States + StoredStateCount, 0.0f);
    hxv_biquad_t biquad = {
        Coefficients,  States, Nchannel, Nstage * Nchannel / 32,
        Nchannel / 32, Nstage, 0,        0};
    hvx_biquad_ripple<DF1>(&biquad, Input, Nchannel, Output, Nchannel,
                           BlockSize);
  }

  bool verify() const override {
    return isclose(SampleCount, Output, Reference) &&
           isclose(StoredStateCount, States, ReferenceStates);
  }
};

DefineTest<BiquadScratchpadTest<4, 32>>
    TBiquadScratchpadDF1("BiquadScratchpad.DF1<NStg=4,NCh=32>");
#else
template <unsigned Nstage, unsigned Nchannel>
class BiquadAlignedTest : public Test {
  static constexpr uint32_t BlockSize = 160;
  static constexpr uint32_t StateCount = 4;
  static constexpr std::size_t SampleCount = BlockSize * Nchannel;
  static constexpr std::size_t CoefficientCount = 5 * Nstage * Nchannel;
  static constexpr std::size_t StoredStateCount =
      StateCount * Nstage * Nchannel;

  float Input[SampleCount];
  float Output[SampleCount];
  float Reference[SampleCount];
  alignas(128) float Coefficients[CoefficientCount];
  alignas(128) float States[StoredStateCount];
  float ReferenceStates[StoredStateCount];

public:
  BiquadAlignedTest(TestFramework &TestFramework) : Test(TestFramework) {
    std::fill(Output, Output + SampleCount, 0.0f);
    std::fill(Reference, Reference + SampleCount, 0.0f);
    std::fill(States, States + StoredStateCount, 0.0f);
    std::fill(ReferenceStates, ReferenceStates + StoredStateCount, 0.0f);
    for (float &sample : Input)
      sample = static_cast<float>(randn());

    static constexpr float CoefficientValues[5] = {0.29292f, 0.5858f, 0.2929f,
                                                   0.0f, 0.17161f};
    for (std::size_t coefficient = 0; coefficient < 5; ++coefficient)
      for (std::size_t stage = 0; stage < Nstage; ++stage)
        for (std::size_t channel = 0; channel < Nchannel; ++channel)
          Coefficients[coefficient * Nstage * Nchannel + stage * Nchannel +
                       channel] = CoefficientValues[coefficient];

    scalar_biquad_df1(Reference, ReferenceStates, Input, Coefficients,
                      BlockSize, Nchannel, Nstage);
  }

  void run(unsigned) override {
    std::fill(Output, Output + SampleCount, 0.0f);
    std::fill(States, States + StoredStateCount, 0.0f);
    hxv_biquad_t biquad = {
        Coefficients,  States, Nchannel, Nstage * Nchannel / 32,
        Nchannel / 32, Nstage, 0,        0};
    hvx_biquad_ripple<DF1>(&biquad, Input, Nchannel, Output, Nchannel,
                           BlockSize);
  }

  bool verify() const override {
    return isclose(SampleCount, Output, Reference) &&
           isclose(StoredStateCount, States, ReferenceStates);
  }
};

DefineTest<BiquadAlignedTest<4, 32>>
    TBiquadAlignedDF1("BiquadAligned.DF1<NStg=4,NCh=32>");
#endif

} // namespace
