# Each group is a separate executable so CTest can run independent simulator
# processes concurrently. Keep aggregate targets for building and running all
# vector kernels.
add_custom_target(vectorkernels)
add_custom_target(test-vectorkernels
  COMMAND ${CMAKE_CTEST_COMMAND} --build-config "$<CONFIG>"
          --parallel "${RIPPLE_TEST_SUITE_TEST_JOBS}"
          --output-on-failure -R "^vectorkernels_"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  VERBATIM
  USES_TERMINAL)

function(add_vectorkernel_group group)
  set(target "vectorkernels_${group}")
  add_test_app(${target} "../../main.sample.cpp" ${ARGN})
  target_compile_options(${target} PRIVATE
    -fenable-ripple
    -mllvm -ripple-pad-to-target-simd)
  target_include_directories(${target} PRIVATE
    ${RIPPLE_TEST_SUITE_INCLUDE_DIR})
  set_tests_properties(${target} PROPERTIES LABELS "vectorkernels")
  add_dependencies(vectorkernels ${target})
  add_dependencies(test-vectorkernels ${target})
endfunction()

add_vectorkernel_group(fft
  fft.cpp)

add_vectorkernel_group(convolution
  conv.cpp
  conv1d.cpp
  transpose_conv3d.cpp)

add_vectorkernel_group(filters
  biquad.cpp
  biquad_scratchpad.cpp
  intlfir.cpp
  limiter.cpp)

add_vectorkernel_group(linalg
  cholesky.cpp
  matmul.cpp
  outer.cpp
  dax.cpp)

add_vectorkernel_group(activations
  prelu.cpp
  relu.cpp
  step.cpp
  swish.cpp)

add_vectorkernel_group(primitives
  cpy.cpp
  mul.cpp
  poly.cpp
  red.cpp
  strided_load.cpp
  sub.cpp
  sum.cpp
  vabsdiff.cpp
  vmax.cpp
  vselect.cpp)
