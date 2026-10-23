//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple.h>
#include <stdint.h>

#include <ripple_hvx.h>
#include <stdio.h>
#include <stdlib.h>

#include "hexagon_types.h"

#include <ripple-test-suite/ripple-test-suite.h>

namespace {

const uint8_t data_left_indices[128] __attribute__((aligned(128))) = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
    45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
    60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
    75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
    90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127};
const uint8_t data_right_indices[128] __attribute__((aligned(128))) = {
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
    143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
    158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
    173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
    188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202,
    203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217,
    218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232,
    233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247,
    248, 249, 250, 251, 252, 253, 254, 255};
const uint8_t const_stage_mask[256] __attribute__((aligned(256))) = {
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
const uint8_t const_stage_mask_reversed[256] __attribute__((aligned(256))) = {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};

#if defined(__hexagon__)
template <int N, typename T>
static __attribute__((always_inline)) T vdeal_hvx(ripple_block_t BS, T input,
                                                  size_t Rt) {
  auto tmp = ripple_to_vec<N, T>(BS, input);
  auto left = Q6_V_lo_W(tmp);
  auto right = Q6_V_hi_W(tmp);

  auto result = Q6_W_vdeal_VVR(right, left, Rt);
  return vec_to_ripple<N, T>(BS, result);
}

template <int N, typename T>
static __attribute__((always_inline)) T vshuff_hvx(ripple_block_t BS, T input,
                                                   size_t Rt) {
  auto tmp = ripple_to_vec<N, T>(BS, input);
  auto left = Q6_V_lo_W(tmp);
  auto right = Q6_V_hi_W(tmp);

  auto result = Q6_W_vshuff_VVR(right, left, Rt);
  return vec_to_ripple<N, T>(BS, result);
}
#endif // __hexagon__

#define _SRC (k / Rt)
#define _OFF (k % Rt)
#define _IDX (_SRC / 2)
#define _MID (N / (2 * Rt))

template <typename T, int N = 128, int Rt = 2, bool mirror = false>
static __attribute__((always_inline)) T vdeal(T input) {
  if (!mirror) {
    input = ripple_shuffle(input, [](size_t k, size_t n) -> size_t {
      if (k < N)
        return k + N;
      else
        return k - N;
    });
  }

  auto _shuffle = [](size_t k, size_t n) -> size_t {
    if (k < N) {
      if (_SRC % 2) {
        if (_IDX < _MID) {
          return (_IDX * 2 * Rt) + _OFF;
        } else {
          return (((_IDX - _MID) * 2 * Rt) + 1) + _OFF;
        }
      } else {
        if (_IDX < _MID) {
          return N + (_IDX * 2 * Rt) + _OFF;
        } else {
          return N + (((_IDX - _MID) * 2 * Rt) + 1) + _OFF;
        }
      }
    } else {
      if (_SRC % 2) {
        if (_IDX < _MID) {
          return (_IDX * 2 * Rt) + _OFF + Rt;
        } else {
          return (((_IDX - _MID) * 2 * Rt)) + _OFF + Rt;
        }
      } else {
        if (_IDX < _MID) {
          return N + (_IDX * 2 * Rt) + _OFF + Rt;
        } else {
          return N + (((_IDX - _MID) * 2 * Rt)) + _OFF + Rt;
        }
      }
    }
  };

  input = ripple_shuffle(input, _shuffle);

  if (mirror) {
    input = ripple_shuffle(input, [](size_t k, size_t n) -> size_t {
      if (k < N)
        return k + N;
      else
        return k - N;
    });
  }

  return input;
}

#define EXPAND(x)                                                              \
  if (Rt & (x))                                                                \
    input = vdeal<T, N, x, mirror>(input);

template <typename T, int N = 128, bool mirror = 0>
static __attribute__((always_inline)) T vdeal(T input, int Rt) {
#if N > 64
  EXPAND(64)
#endif
#if N > 32
  EXPAND(32)
#endif
#if N > 16
  EXPAND(16)
#endif
#if N > 8
  EXPAND(8)
#endif
#if N > 4
  EXPAND(4)
#endif
#if N > 2
  EXPAND(2)
#endif
#if 1
  EXPAND(1)
#endif

  return input;
}

template <typename T, int N = 128, int mirror = 0>
static __attribute__((always_inline)) T vshuff(T input, int Rt) {
#if 1
  EXPAND(1)
#endif
#if N > 2
  EXPAND(2)
#endif
#if N > 4
  EXPAND(4)
#endif
#if N > 8
  EXPAND(8)
#endif
#if N > 16
  EXPAND(16)
#endif
#if N > 32
  EXPAND(32)
#endif
#if N > 64
  EXPAND(64)
#endif

  return input;
}

template <typename T, int N = 128>
static __attribute__((always_inline)) void pack(ripple_block_t BS, T *pack,
                                                T *left, T *right) {
  size_t v = ripple_id(BS, 0);
  if (v < N)
    pack[v] = ripple_shuffle(left[v], [](size_t k, size_t n) { return k; });
  else
    pack[v] =
        ripple_shuffle(right[v - N], [](size_t k, size_t n) { return k; });
}

template <typename T, int N = 128>
static __attribute__((always_inline)) void pack(ripple_block_t BS, T *pack,
                                                const T *left, const T *right) {
  size_t v = ripple_id(BS, 0);
  if (v < N)
    pack[v] = ripple_shuffle(left[v], [](size_t k, size_t n) { return k; });
  else
    pack[v] =
        ripple_shuffle(right[v - N], [](size_t k, size_t n) { return k; });
}

template <typename T, int N = 128>
static __attribute__((always_inline)) void unpack(ripple_block_t BS, T *pack,
                                                  T *left, T *right) {
  size_t v = ripple_id(BS, 0);
  if (v < N)
    left[v] = ripple_shuffle(pack[v], [](size_t k, size_t n) { return k; });
  else
    right[v - N] =
        ripple_shuffle(pack[v], [](size_t k, size_t n) { return k; });
}

template <int N = 128>
static __attribute__((always_inline)) size_t swap_shuffle_fn(size_t i,
                                                             size_t n) {
  return i < N ? i + N : i - N;
}

template <int Stride>
static __attribute__((always_inline)) size_t xor_shuffle_fn(size_t i,
                                                            size_t n) {
  return i ^ Stride;
}

template <typename T, int Size, int Stride> struct BitonicMerge {
  static __attribute__((always_inline)) void
  apply(T &value, uint8_t &index, size_t lane, bool descending) {
    const T other_value = ripple_shuffle(value, xor_shuffle_fn<Stride>);
    const uint8_t other_index = ripple_shuffle(index, xor_shuffle_fn<Stride>);
    const bool other_before =
        descending ? (other_value > value ||
                      (other_value == value && other_index < index))
                   : (other_value < value ||
                      (other_value == value && other_index < index));
    const bool lower_lane = (lane & Stride) == 0;
    const bool forward_stage = (lane & Size) == 0;
    if (other_before == (lower_lane == forward_stage)) {
      value = other_value;
      index = other_index;
    }
    BitonicMerge<T, Size, Stride / 2>::apply(value, index, lane, descending);
  }
};

template <typename T, int Size> struct BitonicMerge<T, Size, 0> {
  static __attribute__((always_inline)) void apply(T &, uint8_t &, size_t,
                                                   bool) {}
};

template <typename T, int Size, int Total, bool Done = (Size > Total)>
struct BitonicStages {
  static __attribute__((always_inline)) void
  apply(T &value, uint8_t &index, size_t lane, bool descending) {
    BitonicMerge<T, Size, Size / 2>::apply(value, index, lane, descending);
    BitonicStages<T, Size * 2, Total>::apply(value, index, lane, descending);
  }
};

template <typename T, int Size, int Total>
struct BitonicStages<T, Size, Total, true> {
  static __attribute__((always_inline)) void apply(T &, uint8_t &, size_t,
                                                   bool) {}
};

template <typename T, int N = 128, int stages = 8>
static __attribute__((always_inline)) void
bitonic_sort_ripple(T *left, T *right, uint8_t mask_to_decide) {
#if !defined(__hexagon__)
  ripple_block_t BS = ripple_set_block_shape(0, 2 * N);
  size_t v = ripple_id(BS, 0);
  T data[2 * N];
  uint8_t indices[2 * N];
  pack<T, N>(BS, data, left, right);
  pack<uint8_t, N>(BS, indices, data_left_indices, data_right_indices);

  T value = data[v];
  uint8_t index = indices[v];
  BitonicStages<T, 2, 2 * N>::apply(value, index, v, mask_to_decide != 0);
  data[v] = value;
  unpack<T, N>(BS, data, left, right);
#else
  int current, factor = 1, val = 127;
  constexpr int counter = ((stages + 1) < 8) ? (stages + 1) : 8;

  ripple_block_t BS = ripple_set_block_shape(0, 2 * N);
  size_t v = ripple_id(BS, 0);

  auto p0 = (v < N);

  uint8_t stage_mask;
  if (mask_to_decide == 1) {
    stage_mask = const_stage_mask_reversed[v];
  } else {
    stage_mask = const_stage_mask[v];
  }

  T data[2 * N];
  pack<T, N>(BS, data, left, right);

  uint8_t indices[2 * N];
  pack<uint8_t, N>(BS, indices, data_left_indices, data_right_indices);

  T first = data[v];
  uint8_t index = indices[v];

  first = vdeal<T, N>(first, 127);
  index = vdeal<uint8_t, N>(index, 127);

  for (int i = 1; i < counter; i++) {
    auto stage_mask_pred = (stage_mask > 0) ? 1 : 0;
    current = factor * 2;

    first = vshuff<T, N>(first, -1);
    index = vshuff<uint8_t, N>(index, -1);

#if defined(__hexagon__)
    first = vdeal_hvx<256, uint8_t>(BS, first, val);
    index = vdeal_hvx<256, uint8_t>(BS, index, val);
#else
    first = vdeal<T, 256>(first, val);
    index = vdeal<uint8_t, 256>(index, val);
#endif

    for (int j = 0; j < (i - 1); j++) {
      T second = ripple_shuffle(first, swap_shuffle_fn);
      uint8_t mirror = ripple_shuffle(index, swap_shuffle_fn);

      auto p1 = (first == second);

      auto p2_1 = (p0 & p1) & (mirror < index);
      auto p2_2 = ((!p0) & p1) & (index < mirror);

      auto p3_1 = p2_1 | (p0 & (first > second));
      auto p3_2 = p2_2 | ((!p0) & (second > first));

      auto exchange_mask = p3_1 | p3_2;
      auto final_mask = stage_mask_pred ^ exchange_mask;
      if (final_mask) {
        first = second;
        index = mirror;
      }

#if defined(__hexagon__)
      first = vshuff_hvx<256, uint8_t>(BS, first, (current / 4));
      index = vshuff_hvx<256, uint8_t>(BS, index, (current / 4));
#else
      first = vshuff<T, 256>(first, (current / 4));
      index = vshuff<uint8_t, 256>(index, (current / 4));
#endif

      current /= 2;
    }

    T second = ripple_shuffle(first, swap_shuffle_fn);
    uint8_t mirror = ripple_shuffle(index, swap_shuffle_fn);

    auto p1 = (first == second);

    auto p2_1 = (p0 & p1) & (mirror < index);
    auto p2_2 = ((!p0) & p1) & (index < mirror);

    auto p3_1 = p2_1 | (p0 & (first > second));
    auto p3_2 = p2_2 | ((!p0) & (second > first));

    auto exchange_mask = p3_1 | p3_2;
    auto final_mask = stage_mask_pred ^ exchange_mask;

    if (final_mask) {
      first = second;
      index = mirror;
    }

    val -= factor;
    factor = factor << 1;

    auto _shuf = [](size_t k, size_t n) {
      if (k % 2) {
        return (n / 2) + (k / 2);
      } else {
        return k / 2;
      }
    };
    stage_mask = ripple_shuffle(stage_mask, _shuf);
  }

  if (stages == 8) {
    auto stage_mask_pred = 0;
    if (mask_to_decide == 1) {
      stage_mask_pred = !stage_mask_pred;
    }
    current = factor * 2;

    first = vshuff<T>(first, -1);
    index = vshuff<uint8_t>(index, -1);

    for (uint8_t j = 0; j < 7; j++) {
      T second = ripple_shuffle(first, swap_shuffle_fn);
      uint8_t mirror = ripple_shuffle(index, swap_shuffle_fn);

      auto p1 = (first == second);

      auto p2_1 = (p0 & p1) & (mirror < index);
      auto p2_2 = ((!p0) & p1) & (index < mirror);

      auto p3_1 = p2_1 | (p0 & (first > second));
      auto p3_2 = p2_2 | ((!p0) & (second > first));

      auto exchange_mask = p3_1 | p3_2;
      auto final_mask = stage_mask_pred ^ exchange_mask;

      if (final_mask) {
        first = second;
        index = mirror;
      }

#if defined(__hexagon__)
      first = vshuff_hvx<256, uint8_t>(BS, first, (current / 4));
      index = vshuff_hvx<256, uint8_t>(BS, index, (current / 4));
#else
      first = vshuff<T, 256>(first, (current / 4));
      index = vshuff<uint8_t, 256>(index, (current / 4));
#endif

      current /= 2;
    }

    T second = ripple_shuffle(first, swap_shuffle_fn);
    uint8_t mirror = ripple_shuffle(index, swap_shuffle_fn);

    auto p1 = (first == second);

    auto p2_1 = (p0 & p1) & (mirror < index);
    auto p2_2 = ((!p0) & p1) & (index < mirror);

    auto p3_1 = p2_1 | (p0 & (first > second));
    auto p3_2 = p2_2 | ((!p0) & (second > first));

    auto exchange_mask = p3_1 | p3_2;
    auto final_mask = stage_mask_pred ^ exchange_mask;
    if (final_mask) {
      first = second;
      index = mirror;
    }
  }

  first = vshuff<T>(first, -1);
  index = vshuff<uint8_t>(index, -1);

  data[v] = first;
  unpack<T>(BS, data, left, right);
#endif
}

using namespace ripple_test_suite;

class test_bitonic_sort : public Test {
protected:
  static constexpr int N_ELEM = 128;
  static constexpr int descending = 1;
  uint8_t data_left[N_ELEM], data_right[N_ELEM];
  uint8_t data_left_ref[N_ELEM], data_right_ref[N_ELEM];

  struct SortRecord {
    uint8_t value;
    uint8_t index;
  };

  // Locally authored scalar oracle. The two input halves form one 256-element
  // sequence; output ranks 0..127 are written to left and 128..255 to right.
  // Records are ordered by value descending and then original lane ascending.
  // The lane is the metadata carried through the bitonic network, so this tie
  // rule preserves value/metadata association without reusing the network or
  // any target intrinsic.
  static void sort_reference(uint8_t *left, uint8_t *right) {
    SortRecord records[2 * N_ELEM];
    for (int i = 0; i < N_ELEM; ++i)
      records[i] = {left[i], static_cast<uint8_t>(i)};
    for (int i = 0; i < N_ELEM; ++i)
      records[N_ELEM + i] = {right[i], static_cast<uint8_t>(N_ELEM + i)};

    for (int i = 1; i < 2 * N_ELEM; ++i) {
      const SortRecord next = records[i];
      int j = i;
      while (j > 0 && (records[j - 1].value < next.value ||
                       (records[j - 1].value == next.value &&
                        records[j - 1].index > next.index))) {
        records[j] = records[j - 1];
        --j;
      }
      records[j] = next;
    }
    for (int i = 0; i < N_ELEM; ++i) {
      left[i] = records[i].value;
      right[i] = records[N_ELEM + i].value;
    }
  }

  void initialize_and_make_reference() {
    for (int i = 0; i < N_ELEM; ++i) {
      data_left[i] = static_cast<uint8_t>(rand() % 256);
      data_right[i] = static_cast<uint8_t>(rand() % 256);
      data_left_ref[i] = data_left[i];
      data_right_ref[i] = data_right[i];
    }
    sort_reference(data_left_ref, data_right_ref);
  }

public:
  test_bitonic_sort(TestFramework &TestFramework) : Test(TestFramework) {}
  bool verify() const override {
    return isclose(N_ELEM, data_left, data_left_ref, 0.0, 0.0) &&
           isclose(N_ELEM, data_right, data_right_ref, 0.0, 0.0);
  }
};

class test_bitonic_sort_Ripple : public test_bitonic_sort {
public:
  test_bitonic_sort_Ripple(TestFramework &TestFramework)
      : test_bitonic_sort(TestFramework) {
    initialize_and_make_reference();
  }

  void run(unsigned) override {
    bitonic_sort_ripple<uint8_t, 128>(data_left, data_right, descending);
  }
};

DefineTest<test_bitonic_sort_Ripple>
    T_test_bitonic_sort_Ripple("bitonic sort[ripple]");

} // namespace
