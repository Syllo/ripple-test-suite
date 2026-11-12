//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license
// information. SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

// Ported from regression/audio_filters_hvx/fft.cpp in the original Ripple
// test suite (revision c03e53b; last FFT change 9300f34). The single-channel
// FP32 radix-2 butterflies, incremental twiddles, inverse normalization, and
// DIF/product/inverse-DIT convolution sequence are retained.
// The port uses canonical Ripple reshape operations and shuffle maps,
// adds native bit reversal, shares aligned storage, and initializes padding
// and references.
// It also adds direct DIF/inverse and bit-reversal checks, DRAM convolution,
// independent small-transform references, and scoped numerical expectations.
// Use the existing portable vectorkernels app so native and Hexagon cases
// share the same build/test path. Omit the original unused multi-channel
// references, disabled vector experiments, and debug prints to keep the port
// focused on its registered single-channel configuration.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>

#include <ripple-test-suite/ripple-test-suite.h>
#include <ripple.h>

// Keep HVX headers confined to the scatter path so native targets can build.
#ifdef __hexagon__
#include <hexagon_types.h>
#include <ripple/HVX_Scatter_Gather.h>
#endif

using namespace ripple_test_suite;
using complex_t = std::complex<float>;

namespace {

// Preserve the original registered 128-byte software block on every target.
// The fixed 32-float shuffle maps and the 16-sample stage boundary depend on it.
constexpr size_t VectorBytes = 128;
constexpr size_t ComplexLanes = VectorBytes / (2 * sizeof(float));
constexpr unsigned RippleLanes = 0;
constexpr float PI = 3.14159265358979323846f;
#define ALWAYS_INLINE inline __attribute__((always_inline))

// FFT-local equivalents of the original complex.ripple.h helpers. Adjacent
// lanes still hold (re, im), and cplex_mul still forms (ac - bd, ad + bc).
// Keep the file self-contained instead of requiring the original helper
// headers; ordinary expressions and unnamed parameters suit C++11 builds.
#define cplex_idx(B, idx) (2 * (idx) + ripple_id(B, 0))
#define cplex_val(B, re, im) ((ripple_id(B, 0) & 1u) ? (im) : (re))
#define cplex_conj(B) cplex_val(B, 1.0f, -1.0f)

size_t reverse_pair(size_t dst, size_t) { return dst ^ 1u; }
size_t evens_shuff(size_t dst, size_t n) {
  return (dst & 1u) * n + 2 * (dst >> 1);
}
size_t odds_shuff(size_t dst, size_t n) {
  return (dst & 1u) * n + 2 * (dst >> 1) + 1;
}

ALWAYS_INLINE float cplex_mul(float ab, float cd, float conj) {
  float ac_bd = ab * cd * conj;
  float ad_bc = ab * ripple_shuffle(cd, reverse_pair);
  return ripple_shuffle_pair(ac_bd, ad_bc, evens_shuff) +
         ripple_shuffle_pair(ac_bd, ad_bc, odds_shuff);
}

// Retain the original complex broadcast as a single Ripple shuffle. Even
// lanes select the real component of sample Pos; odd lanes select its imaginary
// component.
// This retains the original one-shuffle optimized Hexagon IR.
template <size_t Pos> size_t shfl_bcast_cplex(size_t dst, size_t) {
  return (dst & 1u) + 2 * Pos;
}
template <size_t Pos>
ALWAYS_INLINE float cplex_bcast(float x) {
  return ripple_shuffle(x, shfl_bcast_cplex<Pos>);
}

// Replace the original HVX rotations with fixed Ripple source-lane maps.
// rotate_flat_2d uses (dst + 32 - shift) % 32; lrotate_flat_2d uses
// (dst + shift) % 32. Even shifts preserve the real/imaginary pairs.
// Express the permutations in Ripple so the compiler can lower each target.
template <size_t Shift> size_t rotate_shuff(size_t dst, size_t n) {
  return (dst + Shift) % n;
}
// Splice the last Shift floats of lo before the first n - Shift floats of hi.
template <size_t Shift> size_t lsplice_shuff(size_t dst, size_t n) {
  return dst < Shift ? n - Shift + dst : n + dst - Shift;
}

// Canonical ripple_reshape preserves the 32 floats while changing their view
// from [2, 16] to [32] and back, replacing the original temporary arrays.
// Loop-carried values require processRippleReshape in the compiler to keep
// distinct shape views even when the LLVM vector types match. Reusing the
// source value would overwrite its PHI's shape (clang/test/Ripple/reshape.cpp).
ALWAYS_INLINE float rotate_flat_2d(ripple_block_t B, float x, size_t shift) {
  ripple_block_t flatB = ripple_set_block_shape(RippleLanes, 32);
  float flat = ripple_reshape(flatB, x);
  float rotated;
  switch (shift) {
  case 2:
    rotated = ripple_shuffle(flat, rotate_shuff<30>);
    break;
  case 4:
    rotated = ripple_shuffle(flat, rotate_shuff<28>);
    break;
  case 8:
    rotated = ripple_shuffle(flat, rotate_shuff<24>);
    break;
  default:
    assert(shift == 16);
    rotated = ripple_shuffle(flat, rotate_shuff<16>);
  }
  return ripple_reshape(B, rotated);
}

ALWAYS_INLINE float lrotate_flat_2d(ripple_block_t B, float x, size_t shift) {
  ripple_block_t flatB = ripple_set_block_shape(RippleLanes, 32);
  float flat = ripple_reshape(flatB, x);
  float rotated;
  switch (shift) {
  case 2:
    rotated = ripple_shuffle(flat, rotate_shuff<2>);
    break;
  case 4:
    rotated = ripple_shuffle(flat, rotate_shuff<4>);
    break;
  case 8:
    rotated = ripple_shuffle(flat, rotate_shuff<8>);
    break;
  default:
    assert(shift == 16);
    rotated = ripple_shuffle(flat, rotate_shuff<16>);
  }
  return ripple_reshape(B, rotated);
}

// Portable replacement for flat_hvx_lsplice and its hvx_lsplice call.
// Reshape both inputs to [32], splice with ripple_shuffle_pair, then restore B.
ALWAYS_INLINE float flat_lsplice(ripple_block_t B, ripple_block_t flatB,
                                 float lo, float hi, size_t shift) {
  float flatLo = ripple_reshape(flatB, lo);
  float flatHi = ripple_reshape(flatB, hi);
  float spliced;
  switch (shift) {
  case 2:
    spliced = ripple_shuffle_pair(flatLo, flatHi, lsplice_shuff<2>);
    break;
  case 4:
    spliced = ripple_shuffle_pair(flatLo, flatHi, lsplice_shuff<4>);
    break;
  case 8:
    spliced = ripple_shuffle_pair(flatLo, flatHi, lsplice_shuff<8>);
    break;
  default:
    assert(shift == 16);
    spliced = ripple_shuffle_pair(flatLo, flatHi, lsplice_shuff<16>);
  }
  return ripple_reshape(B, spliced);
}

// Scratch size and offset are measured in floats. All FFT work arrays need
// enough room for a complete vector, including transforms smaller than 16.
float *get_aligned_ptr(size_t &offset, float *base, size_t size,
                       size_t capacity) {
  void *ptr = base + offset;
  size_t space = (capacity - offset) * sizeof(float);
  if (!detail::align(VectorBytes, size * sizeof(float), ptr, space)) {
    std::cerr << "FFT scratch buffer overflow" << std::endl;
    std::abort();
  }
  float *result = static_cast<float *>(ptr);
  offset = result - base + size;
  return result;
}

ALWAYS_INLINE size_t log2(size_t n) {
  // calculate log_n
  assert(n > 0 && (n & (n - 1)) == 0);
  size_t log_n, cur_n;
  for (log_n = 0, cur_n = n; cur_n != 1u; cur_n >>= 1)
    log_n++;
  return log_n;
}

/// \brief Reverse bits of a number smaller than or equal to pow(2, nbits)
/// (zero-padded to fit nbits if required). Naive scalar implementation.
/// The relevant vector implementation with Ripple is bit_rev_perm(). Required
/// for ordering the input for decimation in time (DIT) FFT (fft_ref()) or
/// output in a decimation in frequency (DIF) FFT (fft_dif_ref()) to the DFT
/// reference implementation (dft_ref()).
/// \param n input number
/// \param nbits bit width to reorder.
ALWAYS_INLINE uint32_t bit_rev_perm_scalar(uint32_t n, unsigned nbits) {
  uint32_t result = 0;
  for (unsigned i = 0; i < nbits; i++) {
    result = (result << 1) | (n & 1);
    n = n >> 1;
  }

  return result;
}

/// \brief wm[step] (base twiddle factors) pre-calculation.
/// OPT: can be optimized using sin(2a) and cos(2a) identities.
/// \param base twiddle factors for 2^0, 2^1, .. 2^{log_2(n)} (=n).
template <size_t n> void precalc_wms(complex_t *wms) {
  size_t log_n = log2(n);
  // We're computing coefs for m/2, so we don't need to go up to log_n
  for (size_t step = 1; step <= log_n; step++) {
    size_t m = 1 << step;
    wms[step - 1] = complex_t(cosf(-2.f * PI / m), sinf(-2.f * PI / m));
  }
}

// __________________________ Non-interleaved FFT ______________________________

// Radix-2 sequential implementation, no interleaving, decimation
// in time (DIT). We don't permute the input so any comparison with
// dft_ref() requires proper input ordering.
// Textbook version (Cormen, Leiserson & Rivest)
void fft_ref(size_t log_n, const complex_t *u, complex_t *v) {
  size_t n = 1 << log_n;

  // Copy input to output
  for (size_t i = 0; i < n; i++)
    v[i] = u[i];

  // From Cormen, Leiserson, Rivest
  for (size_t step = 1; step <= log_n; step++) {
    size_t m = 1 << step;
    complex_t wm((std::cos)(-2.f * PI / ((float)m)),
                 (std::sin)(-2.f * PI / ((float)m)));
    for (size_t k = 0; k < n; k += m) {
      complex_t w(1.f, 0.f);
      for (size_t j = 0; j < m / 2; ++j) {
        // butterfly
        complex_t t = w * v[k + j + m / 2];
        complex_t u = v[k + j];
        v[k + j] = u + t;
        v[k + j + m / 2] = u - t;
        w = w * wm;
      }
    }
  }
}

// Radix-2 sequential implementation, no interleaving, decimation in
// frequency (DIF). We don't permute the output so any comparison with
// dft_ref() requires proper output ordering.
void fft_dif_ref(size_t log_n, const complex_t *u, complex_t *v) {
  size_t n = 1 << log_n;

  // Copy input to output
  for (size_t i = 0; i < n; i++)
    v[i] = u[i];

  // DIF, starts with m = n;
  for (size_t step = log_n; step > 0; step--) {
    size_t m = 1 << step;
    complex_t wm{(std::cos)(-2.f * PI / (float)m),
                 (std::sin)(-2.f * PI / (float)m)};

    for (size_t k = 0; k < n; k += m) {
      complex_t w{1.0f, 0.f};
      for (size_t j = 0; j < m / 2; j++) {
        complex_t a = v[k + j];
        complex_t b = v[k + m / 2 + j];
        v[k + j] = (a + b);
        v[k + m / 2 + j] = (a - b) * w;
        w *= wm;
      }
    }
  }
}

/// @brief Forms a vector of twiddle factors based on wm.
/// This vector will get multiplied by other wm factors to form the full set
/// of twiddle factors based on wm. Currently, used for seeding
/// in_mem_twiddle_incremental() which calculates the new twiddle factors
/// based on the previous one and is faster.
ALWAYS_INLINE
float in_mem_twiddle(ripple_block_t B, float wm) {
  constexpr size_t nv = VectorBytes / sizeof(float);
  // We'll work with a flat block here
  ripple_block_t flatB = ripple_set_block_shape(RippleLanes, nv);
  float c1 = ripple_broadcast(B, 0b10, cplex_val(B, 1.0f, 0.f));
  float wmv = ripple_broadcast(B, 0b10, wm);
  float w_m = flat_lsplice(B, flatB, c1, wmv, 2);
  float conj = cplex_conj(B);
  // We use a simplified version of the log algo from in_reg_twiddle
  for (size_t j = 0, exp_j = 1; exp_j < (nv >> 1); ++j) {
    exp_j = 1 << (j + 1);
    float splice_w_m = flat_lsplice(B, flatB, c1, w_m, exp_j);
    w_m = cplex_mul(w_m, splice_w_m, conj);
  }
  return w_m;
}

// Duplicate the array: [a0, a1, ..] -> [a0, a0, a1, a1, ...]. Used in
// in_mem_twiddle_incremental().
size_t dup_shuff(size_t dst, size_t) {
  // 0 -> 0, 1 -> 1,  2 -> 0,  3 -> 1
  return (dst & 1) + 2 * (dst >> 2);
}

// Interleave two complex vectors. For example given v1 = [1, 0, 1, 0, ..]
// and v2 = [re(w), im(w), re(w), im(w), ...] generate v3 = [1, 0, re(w), im(w),
// ..]. Used in in_mem_twiddle_incremental().
size_t alt_shuff(size_t dst, size_t n) {
  // 0 -> 0, 1 -> 1, 2 -> n + 2, 3 -> n + 3, ....
  // 4 -> 4, 5 -> 5, 6 -> n + 6, 7 -> n + 7, ....
  return ((dst >> 1) & 1u) * n + dst;
}

/// @brief Forms a vector of twiddle factors based on the previous set
/// of twiddle factors using the following identities:
/// w_{s+1}^2k      = w_{s}^k * 1        , and
/// w_{s+1}^{2k +1} = w_{s}^k * w_{s+1}^1
ALWAYS_INLINE
float in_mem_twiddle_incremental(ripple_block_t B, float w_s, float w_sp1) {
  float conj = cplex_conj(B);

  // duplicate the w_s: [a0, a1, ..] -> [a_0, a_0, a_1, a_1, ...]
  float dup = ripple_shuffle(w_s, dup_shuff);

  // create the vector [1, w_sp1, 1, w_sp1, ...] in mask
  float c1 = cplex_val(B, 1.0f, 0.f);
  float c1_v = ripple_broadcast(B, 0b10, c1);
  float w_sp1_v = ripple_broadcast(B, 0b10, w_sp1);
  float mask = ripple_shuffle_pair(c1_v, w_sp1_v, alt_shuff);

  // multiply by the vectors
  return cplex_mul(dup, mask, conj);
}

// Reverse bytes within a 4-byte input.
size_t rev_shuff(size_t dst, size_t) {
  // 0 -> 3, 1 -> 2, 2 -> 1, 3 -> 0
  // 4 -> 7, 5 -> 6, 6 -> 5, 7 -> 4
  // i.e., dst -> dst - (dst % 4) + 3 - (dst % 4)
  // i.e., dst -> dst + 3 - 2 * (dst % 4) and
  // dst % 4 = dst - (dst >> 2) << 2;
  return dst + 3 - ((dst - ((dst >> 2) << 2)) << 1);
}

uint32_t reverse_bit_u32(uint32_t offset, size_t logn) {
  constexpr size_t nv1 = VectorBytes / (2 * sizeof(float));

  ripple_block_t BS = ripple_set_block_shape(RippleLanes, 2 * nv1);
  ripple_block_t BD = ripple_set_block_shape(RippleLanes, (2 * nv1) * 4);

  // interpret u32 as a group of 4 x u8 and reverse the 4 x u8 values first.
  uint8_t out = ripple_reinterp_u8(BD, offset);
  out = ripple_shuffle(out, rev_shuff);

  // reverse each u8 separately. To do so, we reverse the 4 upper
  // and lower bits first, then reverse 2 bits in each of those
  // two 4 bit chunks. Finally reverse bits which are adjacent to
  // each other. Below are the masks for those operations:
  // F0 = 11110000, 0F = 00001111
  // CC = 11001100, 33 = 00110011
  // AA = 10101010, 55 = 01010101
  out = (((out & 0xF0)) >> 4) | (((out & 0x0F)) << 4);
  out = (((out & 0xCC)) >> 2) | (((out & 0x33)) << 2);
  out = (((out & 0xAA)) >> 1) | (((out & 0x55)) << 1);

  // okay we reversed the u32 but we only care about the left-most
  // log(n) bits out of the 32 bits. So, we right shift by 32 -logn.
  uint32_t rev = ripple_reinterp_u32(BS, out);
  rev = rev >> (32 - logn);
  return rev;
}

/// \brief Reorder the input in `orig` array and writes the reordered output
/// to the `ordered` array. On Hexagon, `ordered` must reside in VTCM.
void bit_rev_perm(size_t n, size_t log_n, const float *orig, float *ordered) {
  constexpr size_t nv1 = VectorBytes / (2 * sizeof(float));

  ripple_block_t BS = ripple_set_block_shape(RippleLanes, 2 * nv1);

#ifdef __hexagon__
  // HVX scatter destinations must reside in VTCM.
  const uint64_t *orig_cplx = reinterpret_cast<const uint64_t *>(orig);
  uint64_t *ordered_cplx = reinterpret_cast<uint64_t *>(ordered);
  ripple_parallel(BS, 0);
  for (uint32_t i = 0; i < n; i++)
    hvx_scatter(ordered_cplx, reverse_bit_u32(i, log_n), orig_cplx[i], n);

  // Retain the original release/dummy-load synchronization. The port adds
  // the memory clobber so the compiler also accounts for the memory effect.
  asm volatile("vmem(%0 + #0):scatter_release\n" ::"r"(ordered_cplx)
               : "memory");
  volatile HVX_Vector sync_dummy = *(HVX_Vector *)ordered_cplx;
  (void)sync_dummy;
#else
  // Native targets lack HVX scatter, so read in bit-reversed order and store
  // contiguously.
  // Bit reversal is its own inverse, so this matches the Hexagon scatter's
  // ordered[reverse_bits(i)] = orig[i] mapping for complete complex pairs.
  ripple_parallel(BS, 0);
  for (uint32_t i = 0; i < n; i++) {
    uint32_t src = reverse_bit_u32(i, log_n);
    ordered[2 * i] = orig[2 * src];
    ordered[2 * i + 1] = orig[2 * src + 1];
  }
#endif
}

/// \brief Creates a vector of w_m coefficients
/// to be multiplied with the A[k + j + m/2] values.
/// [wm^j, for j in 0, m/2 - 1]
/// Since we are doing multiple in-register FFTs on blocks of size 2*m_2.
/// For m_2 == 4, the result looks like
/// [0, 0, 0, 0, 1, wm, wm^2, wm^3, 0, 0, 0, 0, 1, wm, wm^2, wm^3]
/// where each element is a complex, i.e. a pair of contiguous values.
/// The factors are computed using a log algorithm.
/// \param B the ripple block, from which we get the ripple block indices.
/// \param wm the base twiddle factor for this \p step
/// \param step the FFT step (cf original algorithm), i.e. log(m)
/// \param a complex with re(c1) = 1, im(c1) = 0.
/// We use Ripple with a first block size of 2 to implicitly
// represent complex numbers as a pair of floats.
ALWAYS_INLINE
float in_reg_twiddle(ripple_block_t B, float wm, size_t step, float c1) {
  size_t m_2 = 1 << (step - 1);
  size_t v1 = ripple_id(B, 1);
  size_t nv0 = ripple_get_block_size(B, 0);
  // create [1, 1, 1, 1, 1, wm, wm, wm, 1, 1, 1, 1, 1, wm, wm, wm]
  // (illustrated above with m_2 == 4)
  float w_m =
      (((v1 & m_2) == 0) | ((v1 & (m_2 - 1)) == 0))
          ? ripple_broadcast(B, 0b10, c1)
          : ripple_broadcast(B, 0b10, wm); // take them from [2] to [2x16] shape
  float conj = cplex_conj(B);

  // compute the wm^j series in step iterations
  for (size_t j = 0; j < step - 1; ++j) {
    size_t exp_j = 1 << j;
    // OPT: nv0 is always 2 --> could use << 1
    // (compiler may be able to find out by itself)
    // Note: this is basically a rotate_to_upper(exp_j * 2)
    float rot_w_m = rotate_flat_2d(B, w_m, exp_j * nv0);
    w_m = cplex_mul(w_m, rot_w_m, conj);
  }
  float zerod_w_m = (v1 & m_2) == 0 ? 0 : w_m;
  return zerod_w_m;
}

/// \brief In-register radix-2 FFT, single-channel, Ripple implementation,
/// Decimation in time (DIT) variant.
/// \param log_n log2(n), where n is the size of the FFT
/// \param u the input Ripple vector
/// \param wms the twiddle factors: [e^(2*PI*i / m) m = 2^s, for s = 1, log_n]
///            Note: can be longer than log_n, but we'll only use the first
///            log_n values
ALWAYS_INLINE
float in_reg_fft(size_t log_n, float u, const float *wms) {
  constexpr size_t n_c_h = ComplexLanes;

  // We dedicate dimension 0 to represent complex numbers, i.e. pairs of float
  // Dimension 1 represents the flattened "j" and "k" dimensions
  // (whose combined size is fixed, even though m grows)
  ripple_block_t B = ripple_set_block_shape(RippleLanes, 2, n_c_h);
  size_t v1 = ripple_id(B, 1);
  float conj = cplex_conj(B);
  float c1 = cplex_val(B, 1.0f, 0.f);

  // From Cormen, Leiserson, Rivest
  for (size_t step = 1; step <= log_n; step++) {
    size_t m_2 = 1 << (step - 1); // m = 1 << step. This is m/2
    // Load pre-computed wm[step]
    float wm = wms[cplex_idx(B, step - 1)];
    // Twiddle factors are computed using in a log(m) algorithm
    // [w.wm^j, for j in 0, m/2 - 1],
    // aligned on v1 & (m/2) == 1 (to match the A[k + j + m/2] chunks)
    // OPT: Define a cheaper twiddle() by going from one twiddle factor to the
    // next
    float w_j = in_reg_twiddle(B, wm, step, c1);
    float t = cplex_mul(w_j, u, conj);
    // t0 is t aligned with the A[k + j] chunks
    float t0 = lrotate_flat_2d(B, t, 2 * m_2);
    // u1 is u aligned with the A[k + j + m/2 chunks]
    float u1 = rotate_flat_2d(B, u, 2 * m_2);
    u = ((v1 & m_2) == 0) ? u + t0 : u1 - t;
  }
  return u;
}

/// \brief In-register radix-2 FFT, single-channel, Ripple implementation,
/// Decimation in frequency (DIF) variant.
/// \param log_n log2(n), where n is the size of the FFT
/// \param u the input Ripple vector
/// \param wms the twiddle factors
ALWAYS_INLINE
float in_reg_fft_dif(size_t log_n, float u, const float *wms) {
  constexpr size_t n_c_h = ComplexLanes;

  ripple_block_t B = ripple_set_block_shape(RippleLanes, 2, n_c_h);
  size_t v1 = ripple_id(B, 1);
  float conj = cplex_conj(B);
  float c1 = cplex_val(B, 1.0f, 0.f);

  for (size_t step = log_n; step > 0; step--) {
    size_t m_2 = 1 << (step - 1);
    size_t m = m_2 << 1;

    // Load pre-computed wm[step]
    float wm = wms[cplex_idx(B, step - 1)];
    // Calculate the twiddle factors in register
    float w_j = in_reg_twiddle(B, wm, step, c1);

    // Calculate the update for the step
    float ur = rotate_flat_2d(B, u, m);
    float ul = lrotate_flat_2d(B, u, m);
    u = ((v1 & m_2) == 0) ? u + ul : cplex_mul(ur - u, w_j, conj);
  }

  return u;
}

/// @brief One FFT step for multi-vector cases. DIT variant. in and out
/// arrays can be aliased to do the step in place.
/// @param log_n log of n, the number of complex samples in the signal
/// @param step step in the FFT algo. Each step works on blocks of size m,
///             where m = 2^step
/// @param wm base twiddle factor for the step
/// @param vec_wm first nv1 powers of wm (wm^0, ... wm^{nv1-1})
/// @param in input signal (for this step)
/// @param out output signal (for this step)
ALWAYS_INLINE
void in_mem_fft_step(size_t log_n, size_t step, float wm, float vec_wm,
                     float *in, float *out) {
  // # Complex elements in a HVX vector
  constexpr size_t nv1 = VectorBytes / (2 * sizeof(float));
  ripple_block_t B = ripple_set_block_shape(RippleLanes, 2, nv1);
  size_t v1 = ripple_id(B, 1);
  float conj = cplex_conj(B);
  float last_wm = cplex_bcast<nv1 - 1>(vec_wm);
  float wm_factor = cplex_mul(last_wm, wm, conj); // wm^(nv)
  size_t m = 1 << step;
  size_t m_2 = m >> 1;
  size_t n = 1 << log_n;

  // OPT: permuting k and v doesn't improve perf on the simulator. Try on HW
  //      It also makes the code less cache-friendly, presumably.
  // OPT: we could also keep this loop structure and pre-compute the w's.
  for (size_t k = 0; k < n; k += m) {
    float w = vec_wm;
    for (size_t v = 0; v < m_2; v += nv1) {
      float *in2 = ripple_ptr_alignment(&in[2 * (k + v + m_2)], VectorBytes);
      float t = cplex_mul(w, in2[cplex_idx(B, v1)], conj);
      float *in1 = ripple_ptr_alignment(&in[2 * (k + v)], VectorBytes);
      float u = in1[cplex_idx(B, v1)];
      float *out1 = ripple_ptr_alignment(&out[2 * (k + v)], VectorBytes);
      float *out2 = ripple_ptr_alignment(&out[2 * (k + v + m_2)], VectorBytes);
      out1[cplex_idx(B, v1)] = u + t;
      out2[cplex_idx(B, v1)] = u - t;
      // For reference, without alignment hints:
      // out[cplex_idx(B, k + v + v1)] = u + t;
      // out[cplex_idx(B, k + v + m_2 + v1)] = u - t;
      w = cplex_mul(w, wm_factor, conj);
    }
  }
}

/// @brief One FFT step for multi-vector cases. DIF variant. in and out
/// arrays can be aliased to do the step in place.
/// @param log_n log of n, the number of complex samples in the signal
/// @param step step in the FFT algo. Each step works on blocks of size m,
///             where m = 2^step
/// @param wms an array of pre-computed e^(i*2*PI/m)
/// @param in input signal (for this step)
/// @param out output signal (for this step)
ALWAYS_INLINE
void in_mem_fft_dif_step(size_t log_n, size_t step, const float *wms,
                         const float *in, float *out) {
  constexpr size_t nv1 = VectorBytes / (2 * sizeof(float));
  ripple_block_t B = ripple_set_block_shape(RippleLanes, 2, nv1);

  size_t m_2 = 1 << (step - 1);
  size_t m = m_2 << 1;
  size_t n = 1 << log_n;

  float wm = wms[cplex_idx(B, step - 1)];
  float wmv = in_mem_twiddle(B, wm);

  float conj = cplex_conj(B);
  float last_wm = cplex_bcast<nv1 - 1>(wmv);
  float wmf = cplex_mul(last_wm, wm, conj);

  size_t v1 = ripple_id(B, 1);
  for (size_t k = 0; k < n; k += m) {
    float w = wmv;
    for (size_t v = 0; v < m_2; v += nv1) {
      const float *in2 =
          ripple_ptr_alignment(&in[2 * (k + v + m_2)], VectorBytes);
      const float t = in2[cplex_idx(B, v1)];
      const float *in1 = ripple_ptr_alignment(&in[2 * (k + v)], VectorBytes);
      const float u = in1[cplex_idx(B, v1)];
      float *out1 = ripple_ptr_alignment(&out[2 * (k + v)], VectorBytes);
      float *out2 = ripple_ptr_alignment(&out[2 * (k + v + m_2)], VectorBytes);
      out1[cplex_idx(B, v1)] = u + t;
      out2[cplex_idx(B, v1)] = cplex_mul(u - t, w, conj);
      w = cplex_mul(w, wmf, conj);
    }
  }
}

/// @brief Single-channel, complex (im/re interleaved), power-of-two-sized FFT.
/// Decimation in time (DIT) variant.
/// @param log_n Log of the signal size (n)
/// @param wms pre-computed twiddle factors:
///        [e^(2*PI*i / m) m = 2^s, for s = 1, log_n]
/// @param in the input complex signal in bit-reversed order.
/// @param out output transform in natural order.
/// @param tmp a temp array used to compute the intermediary steps.
///        tmp can be a VTCM address if available.
/// @param normalize Normalize by the number of inputs (used for IFFT)
void fft(size_t log_n, const float *wms, const float *in, float *out,
         float *tmp, bool normalize = false) {
  constexpr size_t nv1 = VectorBytes / (2 * sizeof(float));
  size_t log_nv1 = log2(nv1);

  ripple_block_t B = ripple_set_block_shape(RippleLanes, 2, nv1);
  size_t v1 = ripple_id(B, 1);
  float conj = cplex_conj(B);
  size_t n = 1 << log_n;

  float scale = 1.0f;
  if (normalize)
    scale = 1.0f / (float)n;

  // For FFTs that fit in registers
  if (log_n <= log_nv1) {
    out[cplex_idx(B, v1)] =
        scale * in_reg_fft(log_n, in[cplex_idx(B, v1)], wms);
    return;
  }

  // Registers hold nv1 complex vals
  // In-register FFTs give us a FFT up to m = nv1.
  // Use them to calculate the FFT step for m = 2 * nv1 (2 registers).
  float w_nv = in_mem_twiddle(B, wms[cplex_idx(B, log_nv1)]);
  // If this is the last step, write the result directly to out.
  float *tmp_or_out = log_nv1 + 1 == log_n ? out : tmp;
  for (size_t k = 0; k < n; k += 2 * nv1) {
    float t = in[cplex_idx(B, k + v1 + nv1)];
    float reg_t = cplex_mul(w_nv, in_reg_fft(log_nv1, t, wms), conj);
    float u = in[cplex_idx(B, k + v1)];
    float reg_u = in_reg_fft(log_nv1, u, wms);
    tmp_or_out[cplex_idx(B, k + v1)] = scale * (reg_u + reg_t);
    tmp_or_out[cplex_idx(B, k + v1 + nv1)] = scale * (reg_u - reg_t);
  }

  // Port fix: the two-vector case (N = 32) has already written the result.
  // The original continued to read wms[5] for an unused 64-point stage, even
  // though preparation had initialized only wms[0] through wms[4].
  if (log_nv1 + 1 == log_n)
    return;

  float wm = wms[cplex_idx(B, log_nv1 + 2 - 1)];
  float wmv = in_mem_twiddle_incremental(B, w_nv, wm);
  // Then we compute multi-register FFT steps from m = 2 * nv1 to n
  for (size_t step = log_nv1 + 2; step < log_n; ++step) {
    in_mem_fft_step(log_n, step, wm, wmv, tmp, tmp);
    wm = wms[cplex_idx(B, step)];
    wmv = in_mem_twiddle_incremental(B, wmv, wm);
  }
  // Then we write the last step to out
  if (log_nv1 + 2 <= log_n) {
    in_mem_fft_step(log_n, log_n, wm, wmv, tmp, out);
  }
}

/// @brief Single-channel, complex (im/re interleaved), power-of-two-sized FFT.
/// Decimation in frequency (DIF) variant.
/// @param log_n Log of the signal size (n)
/// @param wms pre-computed twiddle factors:
///        [e^(2*PI*i / m) m = 2^s, for s = 1, log_n]
/// @param in the input complex signal in natural order.
/// @param out output transform in bit-reversed order.
/// @param tmp a temp array used to compute the intermediary steps.
///        tmp can be a VTCM address if available.
/// @param normalize Normalize by the number of inputs (used for IFFT)
void fft_dif(size_t log_n, const float *wms, const float *in, float *out,
             float *tmp, bool normalize = false) {
  constexpr size_t nv1 = VectorBytes / (2 * sizeof(float));
  size_t log_nv1 = log2(nv1);

  ripple_block_t B = ripple_set_block_shape(RippleLanes, 2, nv1);
  size_t v1 = ripple_id(B, 1);
  size_t n = 1 << log_n;

  float scale = 1.0f;
  if (normalize)
    scale = 1.0f / (float)n;

  // We use wrk to propagate the upto date input to in_mem_fft_dif_step()
  // and in_reg_fft_dif();
  const float *wrk = in;

  // if log_n > log_nv1, we do the first in_mem_fft_dif_step() to bring the in
  // array to tmp. If tmp is in VTCM, rest of the steps then happen on the VTCM.
  size_t step = log_n;
  if (step > log_nv1) {
    in_mem_fft_dif_step(log_n, step--, wms, wrk, tmp);

    // Do rest of the FFT in memory using the tmp buffer (potentially in VTCM).
    while (step > log_nv1)
      in_mem_fft_dif_step(log_n, step--, wms, tmp, tmp);

    // We assign wrk to tmp at the end so it can be used in in_reg_fft_dif().
    wrk = tmp;
  }

  // Now, step <= log_nv1 and it will fit in the register.
  for (size_t i = 0; i < n; i += nv1) {
    out[cplex_idx(B, i + v1)] =
        scale * in_reg_fft_dif(step, wrk[cplex_idx(B, i + v1)], wms);
  }
}

// Convolution reference implementation.
template <typename T>
void convolution_ref(size_t n, const T *A, const T *B, T *C) {
  const size_t m = n + n - 1;
  for (size_t i = 0; i < m; i++) {
    C[i] = T{};
    for (size_t j = 0; j < n; j++) {
      if ((i >= j) && ((i - j) < n))
        C[i] += A[i - j] * B[j];
    }
  }
}

// Perform the convolution by transforming two input signals into
// frequency domain (by FFT), then calculating the element-wise product
// and finally, transforming the result back to the time domain
// using an IFFT. Scratch needs three buffers of 2 * max(n, ComplexLanes)
// floats each, with room for vector alignment.
void convolution(size_t log_n, const float *wms, const float *wmsi,
                 const float *in0, const float *in1, float *out, float *tmp,
                 size_t tmp_size) {
  constexpr size_t nv1 = VectorBytes / (2 * sizeof(float));
  size_t n = 1 << log_n;
  assert(n >= 2 && "FFT only works for n >= 2");

  size_t vec_len = 2 * (n > nv1 ? n : nv1);
  size_t offset = 0;
  float *tmp0 = get_aligned_ptr(offset, tmp, vec_len, tmp_size);
  float *tmp1 = get_aligned_ptr(offset, tmp, vec_len, tmp_size);
  float *tmp2 = get_aligned_ptr(offset, tmp, vec_len, tmp_size);

  // Calculate output with vector implementation
  // tmp0 = FFT(in0), tmp1 = FFT(in1)
  fft_dif(log_n, wms, in0, tmp0, tmp2);
  fft_dif(log_n, wms, in1, tmp1, tmp2);

  ripple_block_t B = ripple_set_block_shape(RippleLanes, 2, nv1);

  float conj = cplex_conj(B);

  // Retain the original convolution ordering: both DIF spectra are
  // bit-reversed, so their pointwise product feeds inverse DIT directly.
  ripple_parallel(B, 1);
  for (size_t i = 0; i < n; i++) {
    tmp0[cplex_idx(B, i)] =
        cplex_mul(tmp0[cplex_idx(B, i)], tmp1[cplex_idx(B, i)], conj);
  }

  // out = IFFT(tmp0)
  fft(log_n, wmsi, tmp0, out, tmp1, true);
}

// The original FP32 forward DFT was unused by registered tests. This FP64 DFT
// checks small scalar FFT references independently, supports normalized inverse
// transforms, and returns FP32 results in natural order for kernel comparison.
void dft_ref(size_t n, const complex_t *input, complex_t *output,
             bool inverse) {
  const double pi = (std::acos)(-1.0);
  for (size_t k = 0; k < n; ++k) {
    std::complex<double> sum{};
    for (size_t i = 0; i < n; ++i) {
      double angle = (inverse ? 2.0 : -2.0) * pi * k * i / n;
      sum += std::complex<double>(input[i]) *
             std::complex<double>((std::cos)(angle), (std::sin)(angle));
    }
    output[k] = complex_t(inverse ? sum / double(n) : sum);
  }
}

// This is the same v81 IEEE-mode FP32 arithmetic failure tracked by the
// arithmetic and reduction regressions. Keep executing the affected FFT tests;
// bit reversal only uses integer operations and must continue to pass.
// The original FFT tests had no XFAIL overrides. Successful verification in
// this mode is reported as XPASS and fails the framework's check.
std::pair<bool, std::string> fft_arithmetic_xfail() {
#if defined(__hexagon__) && defined(__HVX__) && defined(__HVX_IEEE_FP__) &&    \
    __HEXAGON_ARCH__ == 81
  return {true, "Hexagon v81 IEEE-mode FP32 vector arithmetic produces "
                "incorrect FFT results."};
#else
  return {false, ""};
#endif
}

// Replace repeated per-test allocation with shared, contiguous aligned buffers.
// Test instances are created after framework initialization, so constructors
// can allocate VTCM here; the original allocated its buffers in prepare().
template <size_t N, size_t BufferCount, bool UseVtcm>
class FftStorage : public VTCMTest {
protected:
  static constexpr size_t VecLength = N > ComplexLanes ? N : ComplexLanes;
  static constexpr size_t DramSize =
      BufferCount * 2 * VecLength + VectorBytes / sizeof(float) - 1;
  float Dram[DramSize]{};
  float *Buffers[BufferCount];

  FftStorage(TestFramework &TF) : VTCMTest(TF) {
    static_assert(N > 0 && (N & (N - 1)) == 0,
                  "FFT size must be a power of two");
    // C++11 operator new need not honor an over-aligned member. Reserve
    // padding and align the backing buffer explicitly for the DRAM cases.
    size_t offset = 0;
    float *base =
        get_aligned_ptr(offset, Dram, BufferCount * 2 * VecLength, DramSize);
#ifdef __hexagon__
    if (UseVtcm)
      base = alloc<float>(BufferCount * 2 * VecLength, VectorBytes);
#endif
    // The original reserved full-vector capacity but left inactive lanes
    // uninitialized. Zero every buffer so small FFT loads and convolution
    // input padding are deterministic on both DRAM and VTCM paths.
    for (size_t i = 0; i < BufferCount; ++i) {
      Buffers[i] = base + i * 2 * VecLength;
      std::fill(Buffers[i], Buffers[i] + 2 * VecLength, 0.0f);
    }
  }

  void fill_input(float *input, size_t n) {
    // Retain rand_fill's nominal [-1/n, 1/n] component range. Converting the
    // RNG draw to FP32 earlier can change samples relative to the original.
    for (size_t i = 0; i < 2 * n; ++i)
      input[i] = (2.0f * static_cast<float>(randn()) - 1.0f) / n;
  }
};

template <size_t N, bool Dif, bool Inverse, bool UseVtcm>
class FftTest : public FftStorage<N, 3, UseVtcm> {
  using Base = FftStorage<N, 3, UseVtcm>;
  complex_t Twiddles[32]{};
  complex_t Reference[N]{};
  bool ReferenceCorrect = true;

public:
  FftTest(TestFramework &TF) : Base(TF) {
    precalc_wms<N>(Twiddles);
    if (Inverse)
      for (complex_t &w : Twiddles)
        w = std::conj(w);
  }

  void prepare(unsigned) override {
    this->fill_input(this->Buffers[0], N);
    complex_t input[N];
    for (size_t i = 0; i < N; ++i) {
      input[i] =
          complex_t(this->Buffers[0][2 * i], this->Buffers[0][2 * i + 1]);
      if (Inverse)
        input[i] = std::conj(input[i]);
    }
    if (Dif)
      fft_dif_ref(log2(N), input, Reference);
    else
      fft_ref(log2(N), input, Reference);
    if (Inverse)
      for (complex_t &value : Reference)
        value = std::conj(value) / float(N);

    // Added independent check for all four DIT/DIF forward/inverse variants.
    // DIT needs reordered input; DIF needs reordered expected output. Larger
    // transforms retain the scalar FP32 FFT reference used above.
    // Limit the DFT to small transforms to bound its quadratic reference cost.
    if (N <= 32) {
      complex_t ordered[N], dft[N];
      for (size_t i = 0; i < N; ++i) {
        size_t src = Dif ? i : bit_rev_perm_scalar(i, log2(N));
        ordered[i] =
            complex_t(this->Buffers[0][2 * src], this->Buffers[0][2 * src + 1]);
      }
      dft_ref(N, ordered, dft, Inverse);
      for (size_t i = 0; i < N; ++i) {
        complex_t expected = dft[Dif ? bit_rev_perm_scalar(i, log2(N)) : i];
        ReferenceCorrect &= std::abs(Reference[i] - expected) < 1e-6f;
        Reference[i] = expected;
      }
    }
    // An unwritten valid output must fail against its finite reference value.
    std::fill(this->Buffers[1], this->Buffers[1] + 2 * Base::VecLength, NAN);
  }

  std::pair<bool, std::string> xfail() const override {
    return fft_arithmetic_xfail();
  }

  void run(unsigned) override {
    if (Dif)
      fft_dif(log2(N), reinterpret_cast<float *>(Twiddles), this->Buffers[0],
              this->Buffers[1], this->Buffers[2], Inverse);
    else
      fft(log2(N), reinterpret_cast<float *>(Twiddles), this->Buffers[0],
          this->Buffers[1], this->Buffers[2], Inverse);
  }

  bool verify() const override {
    // The 2048-point forward DIF path accumulates about 2e-6 absolute error
    // against a double precision scalar FFT on legacy Hexagon QFloat. Keep
    // smaller and normalized transforms at the original 1e-6 tolerance.
    // That diagnostic measurement is separate from the registered FP32
    // reference. This tolerance condition applies to every target.
    double atol = Dif && !Inverse && N == 2048 ? 3e-6 : 1e-6;
    return ReferenceCorrect &&
           isclose(2 * N, this->Buffers[1],
                   reinterpret_cast<const float *>(Reference), 1e-6, atol);
  }
};

template <size_t N, bool UseVtcm>
class BitReverseTest : public FftStorage<N, 2, UseVtcm> {
  using Base = FftStorage<N, 2, UseVtcm>;
  float *Ordered;

public:
  BitReverseTest(TestFramework &TF) : Base(TF), Ordered(this->Buffers[1]) {
#ifdef __hexagon__
    // Scatter always needs a VTCM destination, even with a DRAM input.
    if (!UseVtcm)
      Ordered = this->template alloc<float>(2 * Base::VecLength, VectorBytes);
#endif
  }

  void prepare(unsigned) override {
    for (size_t i = 0; i < 2 * N; ++i)
      this->Buffers[0][i] = float(i + 1);
    std::fill(Ordered, Ordered + 2 * Base::VecLength, 0.0f);
  }

  void run(unsigned) override {
    bit_rev_perm(N, log2(N), this->Buffers[0], Ordered);
  }

  bool verify() const override {
    for (size_t i = 0; i < N; ++i) {
      size_t src = bit_rev_perm_scalar(i, log2(N));
      if (Ordered[2 * i] != this->Buffers[0][2 * src] ||
          Ordered[2 * i + 1] != this->Buffers[0][2 * src + 1])
        return false;
    }
    return true;
  }
};

// Retain the original IFftTest's DIT forward/inverse round trip. Standalone
// inverse verification is provided separately by FftTest<..., Inverse=true>.
template <size_t N, bool UseVtcm>
class InverseFftTest : public FftStorage<N, 4, UseVtcm> {
  using Base = FftStorage<N, 4, UseVtcm>;
  complex_t Twiddles[32]{}, InverseTwiddles[32]{};
  float *Ordered;

public:
  InverseFftTest(TestFramework &TF) : Base(TF), Ordered(this->Buffers[3]) {
    precalc_wms<N>(Twiddles);
    for (size_t i = 0; i < log2(N); ++i)
      InverseTwiddles[i] = std::conj(Twiddles[i]);
#ifdef __hexagon__
    // The scatter passes need a VTCM ordering buffer even in DRAM cases.
    if (!UseVtcm)
      Ordered = this->template alloc<float>(2 * Base::VecLength, VectorBytes);
#endif
  }

  void prepare(unsigned) override {
    this->fill_input(this->Buffers[0], N);
    std::fill(Ordered, Ordered + 2 * Base::VecLength, 0.0f);
  }

  std::pair<bool, std::string> xfail() const override {
    return fft_arithmetic_xfail();
  }

  void run(unsigned) override {
    bit_rev_perm(N, log2(N), this->Buffers[0], Ordered);
    fft(log2(N), reinterpret_cast<float *>(Twiddles), Ordered, this->Buffers[1],
        this->Buffers[2]);
    bit_rev_perm(N, log2(N), this->Buffers[1], Ordered);
    fft(log2(N), reinterpret_cast<float *>(InverseTwiddles), Ordered,
        this->Buffers[2], this->Buffers[1], true);
  }

  bool verify() const override {
    return isclose(2 * N, this->Buffers[0], this->Buffers[2], 1e-6, 1e-6);
  }
};

template <size_t N, bool UseVtcm>
class ConvolutionTest : public FftStorage<2 * N, 6, UseVtcm> {
  using Base = FftStorage<2 * N, 6, UseVtcm>;
  complex_t Twiddles[32]{}, InverseTwiddles[32]{};
  complex_t Reference[2 * N]{};

public:
  ConvolutionTest(TestFramework &TF) : Base(TF) {
    precalc_wms<2 * N>(Twiddles);
    for (size_t i = 0; i < log2(2 * N); ++i)
      InverseTwiddles[i] = std::conj(Twiddles[i]);
  }

  void prepare(unsigned) override {
    // Shared storage already supplies zero padding beyond the N input samples.
    // The original used rand_fill(..., 0.f, 0.f), whose a < b assertion would
    // abort setup when assertions were enabled.
    this->fill_input(this->Buffers[0], N);
    this->fill_input(this->Buffers[1], N);
    complex_t lhs[N], rhs[N];
    for (size_t i = 0; i < N; ++i) {
      lhs[i] = complex_t(this->Buffers[0][2 * i], this->Buffers[0][2 * i + 1]);
      rhs[i] = complex_t(this->Buffers[1][2 * i], this->Buffers[1][2 * i + 1]);
    }
    convolution_ref(N, lhs, rhs, Reference);
    // Port fix: convolution_ref writes 2*N-1 samples, but verification checks
    // 2*N. The original left the last reference sample uninitialized; its
    // expected value is zero because it is FFT padding.
    Reference[2 * N - 1] = complex_t{};
    std::fill(this->Buffers[2], this->Buffers[2] + 2 * Base::VecLength, NAN);
  }

  std::pair<bool, std::string> xfail() const override {
    return fft_arithmetic_xfail();
  }

  void run(unsigned) override {
    // The last three adjacent buffers form the convolution's scratch area.
    convolution(log2(2 * N), reinterpret_cast<float *>(Twiddles),
                reinterpret_cast<float *>(InverseTwiddles), this->Buffers[0],
                this->Buffers[1], this->Buffers[2], this->Buffers[3],
                6 * Base::VecLength);
  }

  bool verify() const override {
    return isclose(4 * N, this->Buffers[2],
                   reinterpret_cast<const float *>(Reference), 1e-6, 1e-6);
  }
};

// The original compared two Ripple twiddle computations in verify(), reseeding
// each incremental stage from the direct result. Compare against scalar FP64
// products of the FP32 base twiddles and propagate incremental results in run().
// An independent reference catches errors shared by both Ripple routines;
// carrying results between stages also checks accumulated error. Keep vector
// work in run() so the framework includes it in the measured execution time.
class TwiddleTest : public Test {
  complex_t Twiddles[32]{};
  float Output[7][2 * ComplexLanes]{};
  float Reference[7][2 * ComplexLanes]{};

public:
  TwiddleTest(TestFramework &TF) : Test(TF) {
    precalc_wms<2048>(Twiddles);
    for (size_t s = 4; s < 11; ++s) {
      std::complex<double> w{1.0, 0.0};
      for (size_t i = 0; i < ComplexLanes; ++i) {
        Reference[s - 4][2 * i] = w.real();
        Reference[s - 4][2 * i + 1] = w.imag();
        w *= std::complex<double>(Twiddles[s]);
      }
    }
  }

  std::pair<bool, std::string> xfail() const override {
    return fft_arithmetic_xfail();
  }

  void run(unsigned) override {
    ripple_block_t B = ripple_set_block_shape(RippleLanes, 2, ComplexLanes);
    size_t v1 = ripple_id(B, 1);
    const float *wms = reinterpret_cast<const float *>(Twiddles);
    float w_s = in_mem_twiddle(B, wms[cplex_idx(B, 4)]);
    Output[0][cplex_idx(B, v1)] = w_s;
    for (size_t s = 5; s < 11; ++s) {
      w_s = in_mem_twiddle_incremental(B, w_s, wms[cplex_idx(B, s)]);
      Output[s - 4][cplex_idx(B, v1)] = w_s;
    }
  }

  bool verify() const override {
    return isclose(7 * 2 * ComplexLanes, &Output[0][0], &Reference[0][0], 1e-5,
                   1e-5);
  }
};

DefineTest<TwiddleTest> TTwiddle("FFT.Twiddle<N=2048>");

// Add direct forward DIF, normalized inverse DIT/DIF, and bit-reversal checks
// alongside the original forward DIT and DIT round-trip coverage. Convolution
// now covers DRAM too; native targets register only DRAM variants below.
// Direct checks catch ordering or normalization errors that a round trip could
// cancel. Shared macros keep sizes and memory variants consistent across tests.
#define REGISTER_FFT(N, VTCM, LOCATION)                                        \
  DefineTest<FftTest<N, false, false, VTCM>> TDit##N##LOCATION(                \
      "FFT.DIT<N=" #N "," #LOCATION ">");                                      \
  DefineTest<FftTest<N, true, false, VTCM>> TDif##N##LOCATION(                 \
      "FFT.DIF<N=" #N "," #LOCATION ">");                                      \
  DefineTest<FftTest<N, false, true, VTCM>> TInverseDit##N##LOCATION(          \
      "IFFT.DIT<N=" #N "," #LOCATION ">");                                     \
  DefineTest<FftTest<N, true, true, VTCM>> TInverseDif##N##LOCATION(           \
      "IFFT.DIF<N=" #N "," #LOCATION ">");                                     \
  DefineTest<InverseFftTest<N, VTCM>> TRoundTrip##N##LOCATION(                 \
      "FFT.RoundTrip<N=" #N "," #LOCATION ">");                                \
  DefineTest<BitReverseTest<N, VTCM>> TBitReverse##N##LOCATION(                \
      "FFT.BitReverse<N=" #N "," #LOCATION ">");

#define REGISTER_FFT_SIZES(VTCM, LOCATION)                                     \
  REGISTER_FFT(2, VTCM, LOCATION)                                              \
  REGISTER_FFT(4, VTCM, LOCATION)                                              \
  REGISTER_FFT(8, VTCM, LOCATION)                                              \
  REGISTER_FFT(16, VTCM, LOCATION)                                             \
  REGISTER_FFT(32, VTCM, LOCATION)                                             \
  REGISTER_FFT(64, VTCM, LOCATION)                                             \
  REGISTER_FFT(128, VTCM, LOCATION)                                            \
  REGISTER_FFT(256, VTCM, LOCATION)                                            \
  REGISTER_FFT(512, VTCM, LOCATION)                                            \
  REGISTER_FFT(1024, VTCM, LOCATION)                                           \
  REGISTER_FFT(2048, VTCM, LOCATION)

#define REGISTER_CONV(N, VTCM, LOCATION)                                       \
  DefineTest<ConvolutionTest<N, VTCM>> TConvolution##N##LOCATION(              \
      "FFT.Convolution<N=" #N "," #LOCATION ">");

#define REGISTER_CONV_SIZES(VTCM, LOCATION)                                    \
  REGISTER_CONV(1, VTCM, LOCATION)                                             \
  REGISTER_CONV(2, VTCM, LOCATION)                                             \
  REGISTER_CONV(4, VTCM, LOCATION)                                             \
  REGISTER_CONV(8, VTCM, LOCATION)                                             \
  REGISTER_CONV(16, VTCM, LOCATION)                                            \
  REGISTER_CONV(32, VTCM, LOCATION)                                            \
  REGISTER_CONV(64, VTCM, LOCATION)                                            \
  REGISTER_CONV(128, VTCM, LOCATION)                                           \
  REGISTER_CONV(256, VTCM, LOCATION)                                           \
  REGISTER_CONV(512, VTCM, LOCATION)

REGISTER_FFT_SIZES(false, DRAM)
REGISTER_CONV_SIZES(false, DRAM)
#ifdef __hexagon__
REGISTER_FFT_SIZES(true, VTCM)
REGISTER_CONV_SIZES(true, VTCM)
#endif

} // namespace
