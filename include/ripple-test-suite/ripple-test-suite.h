//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <ripple_math.h>
#include <string>
#include <vector>

namespace ripple_test_suite {

namespace detail {

inline void *align(size_t align, size_t size, void *&ptr, size_t &space) {
  void *result = nullptr;
  if (size <= space) {
    char *begin = static_cast<char *>(ptr);
    char *aligned = reinterpret_cast<char *>(
        reinterpret_cast<uintptr_t>(begin + (align - 1)) & -align);
    size_t padding = static_cast<size_t>(aligned - begin);
    if (padding <= space - size) {
      result = aligned;
      ptr = result;
      space -= padding;
    }
  }
  return result;
}

} // namespace detail

class TestFramework;

class Test {
public:
  virtual ~Test() {}

  Test(TestFramework &TestFramework) : m_TestFramework(TestFramework) {}

  TestFramework &F() const { return m_TestFramework; }

  /**
   * @brief Returns {true, "<REASON-FOR-XFAIL>"} if the test is expected to
   * fail. Expected failures are executed and must fail verification.
   */
  virtual std::pair<bool, std::string> xfail() const { return {false, ""}; }

  /**
   * @brief Returns {true, "<REASON-FOR-SKIP>"} if the test must not run.
   */
  virtual std::pair<bool, std::string> skip() const { return {false, ""}; }

  virtual void prepare(unsigned NumIterations) {}
  virtual void run(unsigned Iteration) = 0;
  virtual bool verify() const = 0;

protected:
  double randn() const;

  TestFramework &m_TestFramework;
};

class TestFramework {
public:
  ~TestFramework();

  void init(int argc, char *argv[]);

  /// @brief: Returns a random number uniformly distributed in [0.0, 1.0)
  double random() const { return (double)rand() / RAND_MAX; }

#ifdef __hexagon__
  /// On Hexagon, tests may use VTCM memory. There is no way to share VTCM
  /// between tests, every test may use the whole allocated VTCM block. This
  /// means that the VTCM memory should only be accessed from the run() function
  /// and its content may not be preserved between test initialization, prepare
  /// and run stages. By default, a 2 MB page of VTCM is allocated but this
  /// amount may be configured by a command line option.
  void *vtcm() const { return m_vtcm; };
  size_t vtcmSize() const { return m_vtcmSize; }
#endif

private:
#ifdef __hexagon__
#ifndef HEXAGONE_STANDALONE
  unsigned int m_vtcmCtx = 0;
#endif
  size_t m_vtcmSize = size_t(2) * 1024 * 1024;
  void *m_vtcm = nullptr;
#endif
};

/**
 * @brief Replicates the logic in np.isclose. Returns true if the N-element
 * array X and the N-element array Y have all elements close to each other.
 *
 * @tparam T Data-type of array's element.
 * @param N
 * @param A
 * @param B
 * @param rtol Relative error tolerance
 * @param atol Absolute error tolerance
 * @return true
 * @return false
 */

template <typename T>
bool isclose(size_t Length, const T *A, const T *B, double rtol = 1e-5,
             double atol = 1e-8) {
  auto abs = [](T a) { return (a > 0) ? a : -a; };
  auto is_close_el = [&](T a, T b) {
    if ((isnan(static_cast<double>(a)) && isinf(static_cast<double>(b))) ||
        (isinf(static_cast<double>(a)) && isnan(static_cast<double>(b)))) {
      return false;
    }
    if (isnan(static_cast<double>(a)) || isnan(static_cast<double>(b)))
      return isnan(static_cast<double>(a)) && isnan(static_cast<double>(b));
    if (isinf(static_cast<double>(a)) || isinf(static_cast<double>(b))) {
      return isinf(static_cast<double>(a)) && isinf(static_cast<double>(b)) &&
             (a > 0) == (b > 0);
    }
    return (abs(a - b) <= (atol + rtol * abs(b)));
  };
  int total_mismatch = 0;
  double max_abs_diff = 0.0;
  for (size_t i = 0; i < Length; i++) {
    if (!is_close_el(A[i], B[i])) {
      double diff = static_cast<double>(abs(A[i] - B[i]));
      // printf("Mismatch at index %zu: ref = %f, val = %f, abs(ref - val) =
      // %f\n",
      //        i, static_cast<double>(A[i]), static_cast<double>(B[i]), diff);
      if (diff > max_abs_diff) {
        max_abs_diff = diff;
      }
      total_mismatch++;
    }
  }
  // printf("Total Mismatch: %d, Max diff: %f\n", total_mismatch, max_abs_diff);
  return (total_mismatch == 0);
}

template <typename T> static bool equal(double eps, const T &X, const T &Y) {
  T d = X - Y;
  if (X != 0)
    d /= X;
  return d < eps && d > -eps;
}
// TODO: rewrite with std::equal
template <typename T, size_t N>
static bool equal(double eps, const T (&X)[N], const T (&Y)[N]) {
  for (size_t i = 0; i != N; ++i)
    if (!equal(eps, X[i], Y[i]))
      return false;
  return true;
}
template <typename T, size_t N, size_t M>
static bool equal(double eps, const T (&X)[N][M], const T (&Y)[N][M]) {
  for (size_t i = 0; i != N; ++i)
    if (!equal(eps, X[i], Y[i]))
      return false;
  return true;
}

class TestSuite {
public:
  TestSuite();
  ~TestSuite();

  void init(int argc, char *argv[]);
  int run();

  // TODO: Rewrite with std::function.
  struct CreateTestBase {
    virtual Test *create(TestFramework &) = 0;
    virtual ~CreateTestBase() {}
  };

  template <typename Test> void addTest(const std::string &TestName) {
    class CreateTest : public CreateTestBase {
      Test *create(TestFramework &F) override { return new Test(F); }
    };
    m_Tests.push_back({TestName, new CreateTest});
  }

  static TestSuite &GetInstance() {
    // The instance is created on the first use.
    static TestSuite instance;
    return instance;
  }

private:
  /// Output stream, which can be redirected by the user.
  std::ostream *m_out;
  std::ofstream m_outStream;

  /// Number of times each test is run.
  int m_repeat = 5;

  TestFramework m_TestFramework;

  // TODO: rewrite using unique_ptr.
  typedef std::pair<std::string, CreateTestBase *> TestInstance;
  std::vector<TestInstance> m_Tests;

  void openOut(const char *);
  std::ostream &out() const { return *m_out; }
};

// TODO: Can use vararg templates, but need std::forward.
template <typename Test> class DefineTest {
public:
  DefineTest(const std::string &TestName) {
    TestSuite::GetInstance().addTest<Test>(TestName);
  }
};

/// Test base for tests that need short-lived, aligned scratch allocations.
/// On Hexagon the allocations come from the test's VTCM page; elsewhere they
/// come from aligned heap memory.
class VTCMTest : public Test {
#ifdef __hexagon__
  void *vtcm_ptr;
#endif

public:
  VTCMTest(TestFramework &TestFramework)
      : Test(TestFramework)
#ifdef __hexagon__
        ,
        vtcm_ptr(TestFramework.vtcm())
#endif
  {
  }

  void *alloc(size_t Size, size_t Align) {
#ifdef __hexagon__
    char *const end = (char *)F().vtcm() + F().vtcmSize();
    size_t space = end - (char *)vtcm_ptr;
    if (!detail::align(Align, Size, vtcm_ptr, space)) {
      std::cerr << "VTCM overflow trying to allocate " << Size
                << " bytes aligned by " << Align << ", current offset "
                << (char *)vtcm_ptr - (char *)F().vtcm() << ", total "
                << F().vtcmSize() << std::endl;
      abort();
    }
    void *ret = vtcm_ptr;
    vtcm_ptr = (char *)vtcm_ptr + Size;
    return ret;
#else
    return std::aligned_alloc(Align, Size);
#endif
  }

  template <typename T> T *alloc(size_t N = 1, size_t Align = alignof(T)) {
    return (T *)alloc(N * sizeof(T), Align);
  }

  template <typename T>
  void alloc(T *&ret, size_t N = 1, size_t Align = alignof(T)) {
    ret = static_cast<T *>(alloc(sizeof(T) * N, Align));
  }
};

inline double Test::randn() const { return F().random(); }

// TODO: Investigate why templates don't work.
#if defined __x86_64__
// template <typename T>
// constexpr size_t RIPPLE_BLOCK_SIZE(const T&) {
//  return 8;
//}
#define RIPPLE_BLOCK_SIZE(T) (8)
#elif defined __HVX__
// template <typename T>
// constexpr size_t RIPPLE_BLOCK_SIZE(const T&) {
//  return __HVX_LENGTH__ / sizeof(T);
//}
#define RIPPLE_BLOCK_SIZE(T) (__HVX_LENGTH__ / sizeof(T))
#elif defined __aarch64__
// TODO: Choose optimal block size based on architecture features such as SVE.
#define RIPPLE_BLOCK_SIZE(T) (8)
#endif

} // namespace ripple_test_suite
