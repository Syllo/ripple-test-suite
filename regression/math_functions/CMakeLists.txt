add_library(math_functions INTERFACE)

target_sources(math_functions PUBLIC
  math_functions.cpp
  scalar_math.cpp
  math_functions_test.cpp
  )
