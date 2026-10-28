add_test_app(vectorkernels
  "../../main.sample.cpp"
  conv1d.cpp
  cpy.cpp
  dax.cpp
  limiter.cpp
  matmul.cpp
  mul.cpp
  outer.cpp
  poly.cpp
  prelu.cpp
  red.cpp
  relu.cpp
  step.cpp
  strided_load.cpp
  sub.cpp
  sum.cpp
  swish.cpp
  vabsdiff.cpp
  vmax.cpp
  vselect.cpp
)

target_compile_options(vectorkernels PRIVATE
  -fenable-ripple
  -mllvm -ripple-pad-to-target-simd
)
target_include_directories(vectorkernels PRIVATE
  ${RIPPLE_TEST_SUITE_INCLUDE_DIR}
)
