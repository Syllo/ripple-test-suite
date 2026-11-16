# Register each group separately so CTest can schedule independent simulator
# processes. Keep aggregate targets for building and running all regressions.
add_custom_target(regressions)
add_custom_target(test-regressions
  COMMAND ${CMAKE_CTEST_COMMAND} --build-config "$<CONFIG>"
          --parallel "${RIPPLE_TEST_SUITE_TEST_JOBS}"
          --output-on-failure -R "^regressions_"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  VERBATIM
  USES_TERMINAL)

function(add_regression_group group)
  set(target "regressions_${group}")
  add_test_app(${target} "${PROJECT_SOURCE_DIR}/main.sample.cpp" ${ARGN})
  target_compile_options(${target} PRIVATE -fenable-ripple)
  target_include_directories(${target} PRIVATE ${RIPPLE_TEST_SUITE_INCLUDE_DIR})
  set_tests_properties(${target} PROPERTIES LABELS "regressions")
  add_dependencies(regressions ${target})
  add_dependencies(test-regressions ${target})
endfunction()

add_regression_group(arithmetic
  arithmetic.cpp
  rsqrt.cpp
  bf16_mult.cpp)

set(REDUCTION_SOURCES reductions.cpp)

# The Hexagon backend cannot yet legalize the 256-lane predicate operations in
# the Ripple bitonic sort across multiple HVX vectors.
if(NOT ${BUILD_TARGET} STREQUAL "Hexagon")
  list(APPEND REDUCTION_SOURCES topk.cpp)
endif()

add_regression_group(reductions ${REDUCTION_SOURCES})

add_subdirectory(vectorkernels)
add_subdirectory(math_functions)

if(RIPPLE_TEST_INCLUDE_HVX_RT_LIB)
  add_subdirectory(scatter_gather)
  add_subdirectory(narrow_shift)
endif()
