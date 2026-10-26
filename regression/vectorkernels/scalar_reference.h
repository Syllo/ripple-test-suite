//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#ifndef RIPPLE_TEST_SUITE_VECTOR_KERNELS_SCALAR_REFERENCE_H
#define RIPPLE_TEST_SUITE_VECTOR_KERNELS_SCALAR_REFERENCE_H

#include <cstddef>

namespace scalar_reference {

template <typename T, typename Operation>
void transform(T *output, const T *input, std::size_t count,
               Operation operation) {
  for (std::size_t i = 0; i < count; ++i)
    output[i] = operation(input[i]);
}

template <typename T, typename Operation>
void transform(T *output, const T *lhs, const T *rhs, std::size_t count,
               Operation operation) {
  for (std::size_t i = 0; i < count; ++i)
    output[i] = operation(lhs[i], rhs[i]);
}

template <typename T, typename Predicate, typename Operation>
void transform_if(T *output, const T *lhs, const T *rhs, std::size_t count,
                  Predicate predicate, Operation operation) {
  for (std::size_t i = 0; i < count; ++i) {
    if (predicate(lhs[i]))
      output[i] = operation(lhs[i], rhs[i]);
  }
}

template <typename T> T sum(const T *input, std::size_t count) {
  T result = T{};
  for (std::size_t i = 0; i < count; ++i)
    result += input[i];
  return result;
}

template <typename T>
void matrix_multiply(T *output, const T *lhs, const T *rhs,
                     std::size_t dimension) {
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t column = 0; column < dimension; ++column) {
      T value = T{};
      for (std::size_t inner = 0; inner < dimension; ++inner)
        value += lhs[row * dimension + inner] * rhs[inner * dimension + column];
      output[row * dimension + column] = value;
    }
  }
}

template <typename T>
void outer_product(T *output, const T *column, const T *row,
                   std::size_t dimension) {
  for (std::size_t i = 0; i < dimension; ++i)
    for (std::size_t j = 0; j < dimension; ++j)
      output[i * dimension + j] = column[i] * row[j];
}

template <typename T>
void split_pairs(const T *input, T *even, T *odd, std::size_t pair_count) {
  for (std::size_t i = 0; i < pair_count; ++i) {
    even[i] = input[2 * i];
    odd[i] = input[2 * i + 1];
  }
}

} // namespace scalar_reference

#endif
