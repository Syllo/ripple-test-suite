//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple.h>

#include <ripple-test-suite/ripple-test-suite.h>

namespace {
// Reordering function: for each destination index i among n,
// defines the source index to take the value from.
// This function can be arbitrarily complicated since it is only
// evaluated by the compiler
size_t pick_rgb(size_t i, size_t n) {
  size_t dst_rgb_idx = i % 3;
  size_t dst_vec_idx = i / 3;
  size_t dst_size = 3 * (n / 4);
  if (i < dst_size)
    return 4 * dst_vec_idx + dst_rgb_idx;
  // the remaining items.
  // Could be anything, but this choice makes the reordering a permutation,
  // which becomes efficient HVX code.
  else
    return 3 + 4 * (i - dst_size);
}

void rgba_to_rgb_ripple(size_t N, const unsigned char *Rgba, unsigned char *Rgb) {
  ripple_block_t BS = ripple_set_block_shape(0, 64);
  size_t v0 = ripple_id(BS, 0);
  size_t nvecs = (N * 4) / 64;

  size_t ivec;
  for (ivec = 0; ivec < nvecs - 1; ++ivec)
    Rgb[48 * ivec + v0] = ripple_shuffle(Rgba[64 * ivec + v0], pick_rgb);
  // The shuffle must run unmasked: pick_rgb sources lanes up to 63, so
  // computing it under the v0 < 48 guard would read zeros from lanes 48-63.
  // The load itself stays in bounds, since 64 * ivec + 63 < 64 * nvecs <= N
  // * 4.
  unsigned char shuf = ripple_shuffle(Rgba[64 * ivec + v0], pick_rgb);
  if (v0 < 48)
    Rgb[48 * ivec + v0] = shuf;
  ++ivec;
  // The leftover N * 4 % 64 input bytes hold N % 16 whole pixels, so the tail
  // produces 3 * (N % 16) output bytes. The shuffle has to sit between the two
  // guards: it needs all 4 * (N % 16) input lanes active, since destination
  // lane i sources lane 4 * (i / 3) + i % 3, up to 4 * (N % 16) - 2 -- past the
  // 3 * (N % 16) store mask. Only the store may be narrowed to 3 * (N % 16).
  if (v0 < (N * 4) % 64) {
    unsigned char tail_shuf = ripple_shuffle(Rgba[64 * ivec + v0], pick_rgb);
    if (v0 < 3 * (((N * 4) % 64) / 4))
      Rgb[48 * ivec + v0] = tail_shuf;
  }
}

void rgba_to_rgb_ref(size_t N, const unsigned char *Rgba, unsigned char *Rgb) {
  for (int i = 0; i < N; i++)
    for (int j = 0; j < 3; j++)
      Rgb[3 * i + j] = Rgba[4 * i + j];
}

} // namespace

namespace ripple_test_suite {

enum KernelT {
  Reference,
  RippleOpt,
};

template <KernelT KT> class RGBAToRGB : public Test {
  static constexpr int N = 11729;
  unsigned char X[N * 4], Y[N * 3], YRef[N * 3];

public:
  RGBAToRGB(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; i++)
      for (int j = 0; j < 4; j++)
        X[i * 4 + j] = randn() * 250;

    rgba_to_rgb_ref(N, X, YRef);
  }

  void run(unsigned) override {
    if (KT == KernelT::Reference)
      rgba_to_rgb_ref(N, X, Y);
    if (KT == KernelT::RippleOpt)
      rgba_to_rgb_ripple(N, X, Y);
  }

  bool verify() const override { return equal(1e-5, Y, YRef); }
};

DefineTest<RGBAToRGB<Reference>> RGBA2RGBTestInstance_0("rgba.ref");
DefineTest<RGBAToRGB<RippleOpt>> RGBA2RGBTestInstance_1("rgba.ripple");

} // namespace ripple_test_suite
