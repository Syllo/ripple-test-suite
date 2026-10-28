//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license
// information. SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <cmath>
#include <cstddef>

#include <ripple-test-suite/ripple-test-suite.h>
#include <ripple.h>

namespace {

static constexpr int CHOL_DIM = 20;
static constexpr int CHOL_SIZE = CHOL_DIM * CHOL_DIM;

template <typename T> struct ComputeTypeHelper {
  using type = float;
};
template <> struct ComputeTypeHelper<double> {
  using type = double;
};

template <typename T> using ComputeType = typename ComputeTypeHelper<T>::type;

static inline int idx2d(int row, int col) { return row * CHOL_DIM + col; }

template <typename T>
void cholesky_ripple(T *aRe, T *aIm, T *lOutRe, T *lOutIm) {
  using CT = ComputeType<T>;
  static constexpr CT kDiagonalLoading = (CT)1.0;
  static constexpr int CHUNK = 8;
  ripple_block_t BS = ripple_set_block_shape(0, CHUNK);

  for (int i = 0; i < CHOL_DIM; ++i) {
    for (int j = 0; j <= i; ++j) {
      CT sumRe = (CT)aRe[idx2d(i, j)];
      CT sumIm = (CT)aIm[idx2d(i, j)];

      if (i == j)
        sumRe += kDiagonalLoading;

      int k = 0;
      for (; k + CHUNK <= j; k += CHUNK) {
        CT partialRe = (CT)0;
        CT partialIm = (CT)0;

        ripple_parallel_full(BS, 0);
        for (int kk = k; kk < k + CHUNK; ++kk) {
          CT likRe = (CT)lOutRe[idx2d(i, kk)];
          CT likIm = (CT)lOutIm[idx2d(i, kk)];
          CT ljkRe = (CT)lOutRe[idx2d(j, kk)];
          CT ljkIm = (CT)lOutIm[idx2d(j, kk)];

          partialRe += likRe * ljkRe + likIm * ljkIm;
          partialIm += likIm * ljkRe - likRe * ljkIm;
        }

        sumRe -= ripple_reduceadd(0b1, partialRe);
        sumIm -= ripple_reduceadd(0b1, partialIm);
      }

      for (; k < j; ++k) {
        CT likRe = (CT)lOutRe[idx2d(i, k)];
        CT likIm = (CT)lOutIm[idx2d(i, k)];
        CT ljkRe = (CT)lOutRe[idx2d(j, k)];
        CT ljkIm = (CT)lOutIm[idx2d(j, k)];

        sumRe -= likRe * ljkRe + likIm * ljkIm;
        sumIm -= likIm * ljkRe - likRe * ljkIm;
      }

      if (i == j) {
        if (sumRe <= (CT)0)
          sumRe = kDiagonalLoading;
        lOutRe[idx2d(i, i)] = (T)sqrt((double)sumRe);
        lOutIm[idx2d(i, i)] = (T)0;
      } else {
        CT denom = (CT)lOutRe[idx2d(j, j)];
        if (denom == (CT)0)
          denom = (CT)1;
        lOutRe[idx2d(i, j)] = (T)(sumRe / denom);
        lOutIm[idx2d(i, j)] = (T)(sumIm / denom);
      }
    }
  }
}

#undef sqrt

template <typename T> struct ScalarComplex {
  T Real;
  T Imaginary;
};

template <typename T>
void scalar_cholesky(const T *matrix_real, const T *matrix_imaginary,
                     T *factor_real, T *factor_imaginary) {
  using Accumulator = ComputeType<T>;

  for (int row = 0; row < CHOL_DIM; ++row) {
    for (int column = 0; column <= row; ++column) {
      const std::size_t element =
          static_cast<std::size_t>(row) * CHOL_DIM + column;
      ScalarComplex<Accumulator> residual{
          static_cast<Accumulator>(matrix_real[element]),
          static_cast<Accumulator>(matrix_imaginary[element])};

      if (row == column)
        residual.Real += Accumulator{1};

      for (int inner = 0; inner < column; ++inner) {
        const std::size_t row_element =
            static_cast<std::size_t>(row) * CHOL_DIM + inner;
        const std::size_t column_element =
            static_cast<std::size_t>(column) * CHOL_DIM + inner;
        const ScalarComplex<Accumulator> row_value{
            static_cast<Accumulator>(factor_real[row_element]),
            static_cast<Accumulator>(factor_imaginary[row_element])};
        const ScalarComplex<Accumulator> column_value{
            static_cast<Accumulator>(factor_real[column_element]),
            static_cast<Accumulator>(factor_imaginary[column_element])};

        // Subtract L(row, inner) * conjugate(L(column, inner)).
        residual.Real -= row_value.Real * column_value.Real +
                         row_value.Imaginary * column_value.Imaginary;
        residual.Imaginary -= row_value.Imaginary * column_value.Real -
                              row_value.Real * column_value.Imaginary;
      }

      if (row == column) {
        if (residual.Real <= Accumulator{0})
          residual.Real = Accumulator{1};
        factor_real[element] =
            static_cast<T>(std::sqrt(static_cast<double>(residual.Real)));
        factor_imaginary[element] = T{};
        continue;
      }

      const std::size_t diagonal =
          static_cast<std::size_t>(column) * CHOL_DIM + column;
      Accumulator divisor = static_cast<Accumulator>(factor_real[diagonal]);
      if (divisor == Accumulator{0})
        divisor = Accumulator{1};
      factor_real[element] = static_cast<T>(residual.Real / divisor);
      factor_imaginary[element] = static_cast<T>(residual.Imaginary / divisor);
    }
  }
}

using namespace ripple_test_suite;

template <typename T, unsigned SigDigits> class CholeskyTest : public Test {
  std::vector<T> aRe, aIm, lOutRe, lOutIm, lRefRe, lRefIm;

public:
  CholeskyTest(TestFramework &TF)
      : Test(TF), aRe(CHOL_SIZE, 0), aIm(CHOL_SIZE, 0), lOutRe(CHOL_SIZE, 0),
        lOutIm(CHOL_SIZE, 0), lRefRe(CHOL_SIZE, 0), lRefIm(CHOL_SIZE, 0) {
    // Build a Hermitian positive-definite matrix: A = B*B^H + I
    std::vector<T> bRe(CHOL_SIZE, 0), bIm(CHOL_SIZE, 0);
    for (int i = 0; i < CHOL_SIZE; i++) {
      bRe[i] = static_cast<T>(2.0 * (randn() - 0.5));
      bIm[i] = static_cast<T>(2.0 * (randn() - 0.5));
    }
    for (int i = 0; i < CHOL_DIM; i++) {
      for (int j = 0; j < CHOL_DIM; j++) {
        T re = 0, im = 0;
        for (int k = 0; k < CHOL_DIM; k++) {
          re += bRe[i * CHOL_DIM + k] * bRe[j * CHOL_DIM + k] +
                bIm[i * CHOL_DIM + k] * bIm[j * CHOL_DIM + k];
          im += bIm[i * CHOL_DIM + k] * bRe[j * CHOL_DIM + k] -
                bRe[i * CHOL_DIM + k] * bIm[j * CHOL_DIM + k];
        }
        aRe[i * CHOL_DIM + j] = re;
        aIm[i * CHOL_DIM + j] = im;
      }
    }
    scalar_cholesky(&aRe[0], &aIm[0], &lRefRe[0], &lRefIm[0]);
  }

  void run(unsigned) override {
    std::fill(lOutRe.begin(), lOutRe.end(), (T)0);
    std::fill(lOutIm.begin(), lOutIm.end(), (T)0);
    cholesky_ripple(&aRe[0], &aIm[0], &lOutRe[0], &lOutIm[0]);
  }

  bool verify() const override {
    return isclose(CHOL_SIZE, &lOutRe[0], &lRefRe[0], pow(0.1, SigDigits)) &&
           isclose(CHOL_SIZE, &lOutIm[0], &lRefIm[0], pow(0.1, SigDigits));
  }
};

DefineTest<CholeskyTest<float, 3>> TCholeskyFloat("Cholesky<float>");

} // namespace
