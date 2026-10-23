//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple.h>

#if __has_bf16__
#include <cmath>
#include <stddef.h>
#include <stdint.h>

namespace {

// Ripple implementation under test.
template <typename T>
void flat_mul_ripple(T *outptr, const T *aptr, const T *bptr, size_t length) {
  ripple_block_t BS = ripple_set_block_shape(0, 64);
  size_t v0 = ripple_id(BS, 0);

  for (size_t i = 0; i < (length >> 6); i++) {
    const T *a = ripple_ptr_alignment(aptr + 64 * i, 128);
    const T *b = ripple_ptr_alignment(bptr + 64 * i, 128);
    T *out = ripple_ptr_alignment(outptr + 64 * i, 128);

    out[v0] = a[v0] * b[v0];
  }
}

// Locally authored scalar oracle: widen each input to binary32, multiply in
// binary32, then convert once to the destination element type.
template <typename T>
void flat_mul_reference(T *outptr, const T *aptr, const T *bptr,
                        size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const float product =
        static_cast<float>(aptr[i]) * static_cast<float>(bptr[i]);
    outptr[i] = static_cast<T>(product);
  }
}

__attribute__((aligned(128))) uint16_t A[17280];
__attribute__((aligned(128))) uint16_t B[17280];
__attribute__((aligned(128))) uint16_t YRef[17280];
__attribute__((aligned(128))) uint16_t Y[17280];

} // namespace

#include <ripple-test-suite/ripple-test-suite.h>
using namespace ripple_test_suite;
enum KernelVariant { Reference, Ripple };

template <KernelVariant KT, typename Dtype, unsigned SigDigits>
class FlatMultTest : public Test {
  static constexpr int N = 17280;

  void init_arrays(int N, Dtype *a, Dtype *b, Dtype *out, Dtype *outref) {
    for (int i = 0; i < N; i++) {
      const float x1 = 32.0f * static_cast<float>(randn() - 0.5);
      const float x2 = 32.0f * static_cast<float>(randn() - 0.5);
      a[i] = static_cast<Dtype>(x1);
      b[i] = static_cast<Dtype>(x2);
      out[i] = outref[i] = static_cast<Dtype>(0.0f);
    }
  }

  bool verify_for(const __bf16 *out, const __bf16 *out_ref) const {
    for (int i = 0; i < N; ++i) {
      const float actual = static_cast<float>(out[i]);
      const float expected = static_cast<float>(out_ref[i]);
      if (!isclose(size_t(1), &actual, &expected, pow(0.1, SigDigits)))
        return false;
    }
    return true;
  }
  bool verify_for(const _Float16 *out, const _Float16 *out_ref) const {
    return isclose(N, out, out_ref, pow(0.1, SigDigits));
  }

public:
  FlatMultTest(TestFramework &TestFramework) : Test(TestFramework) {
    init_arrays(N, reinterpret_cast<Dtype *>(A), reinterpret_cast<Dtype *>(B),
                reinterpret_cast<Dtype *>(Y), reinterpret_cast<Dtype *>(YRef));
    flat_mul_reference(reinterpret_cast<Dtype *>(YRef),
                       reinterpret_cast<Dtype *>(A),
                       reinterpret_cast<Dtype *>(B), N);
  }

  void run(unsigned) override {
    if (KT == Reference)
      flat_mul_reference(reinterpret_cast<Dtype *>(Y),
                         reinterpret_cast<Dtype *>(A),
                         reinterpret_cast<Dtype *>(B), N);
    if (KT == Ripple)
      flat_mul_ripple(reinterpret_cast<Dtype *>(Y),
                      reinterpret_cast<Dtype *>(A),
                      reinterpret_cast<Dtype *>(B), N);
  }

  bool verify() const override {
    return verify_for(reinterpret_cast<const Dtype *>(Y),
                      reinterpret_cast<const Dtype *>(YRef));
  }
};

DefineTest<FlatMultTest<Reference, __bf16, 1>> Bf16RefMul("Multiply<ref,bf16>");
DefineTest<FlatMultTest<Ripple, __bf16, 1>>
    Bf16RippleMul("Multiply<ripple,bf16>");
DefineTest<FlatMultTest<Reference, _Float16, 3>>
    FP16RefMul("Multiply<ref,fp16>");
DefineTest<FlatMultTest<Ripple, _Float16, 3>>
    FP16RippleMul("Multiply<ripple,fp16>");
#endif // __has_bf16__
