//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <cmath>

// Hexagon's math library does not provide the scalar rsqrtf entry point used
// by the Ripple vector candidate. Keep a weak scalar fallback for the loop
// remainder while allowing a target runtime implementation to take precedence.
extern "C" __attribute__((weak)) float rsqrtf(float x) {
  return 1.0f / std::sqrt(x);
}
