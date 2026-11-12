cmake_minimum_required(VERSION 3.14.3)

project(ripple-test-suite CXX)

set(RIPPLE_TEST_SUITE_TEST_JOBS 4 CACHE STRING
    "Maximum number of test executables to run concurrently")
set(RIPPLE_TEST_SUITE_TEST_REPEAT 1 CACHE STRING
    "Number of kernel iterations per test when running CMake test targets")

foreach(test_option RIPPLE_TEST_SUITE_TEST_JOBS RIPPLE_TEST_SUITE_TEST_REPEAT)
  if(NOT "${${test_option}}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "${test_option} must be a positive integer")
  endif()
endforeach()

# The AArch64 SME tests use a fixed streaming vector length. CI can override
# this for the QEMU version it runs with.
set(RIPPLE_TEST_SUITE_SME_VECTOR_LENGTH_BYTES 64 CACHE STRING
    "Streaming SME vector length in bytes for AArch64 tests")

# Detect the target processor. We cannot rely on the compiler/toolchain because one compiler can
# generate output for multiple targets e.g. by setting `--target=` in clang options.
# We use the C++ compiler. While it is possible to configure different compilers for different
# targets, do not do that.
string(REPLACE " " ";" CMAKE_CXX_FLAGS_LIST "${CMAKE_CXX_FLAGS}")
execute_process(COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS_LIST} -dumpmachine
  OUTPUT_VARIABLE target_triple OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY)
string(REPLACE "-" ";" target_triple_list ${target_triple})
list(GET target_triple_list 0 BUILD_TARGET)

# Canonicalize target names according to LLVM target names.
string(TOLOWER "${BUILD_TARGET}" build_target_lower)
if(${build_target_lower} STREQUAL "hexagon")
  set(BUILD_TARGET "Hexagon")
elseif(${build_target_lower} STREQUAL "x86_64")
  set(BUILD_TARGET "X86")
elseif(${build_target_lower} STREQUAL "aarch64")
  set(BUILD_TARGET "AArch64")
else()
  MESSAGE(FATAL_ERROR "Compiler '-dumpmachine' returned \'${target_triple}\', "
          "which is not a supported target triple.")
endif()

MESSAGE(STATUS "Detected target: ${BUILD_TARGET}")

if(${BUILD_TARGET} STREQUAL "AArch64")
  add_compile_definitions(
    RIPPLE_TEST_SUITE_SME_VECTOR_LENGTH_BYTES=${RIPPLE_TEST_SUITE_SME_VECTOR_LENGTH_BYTES})
endif()

if(NOT ${BUILD_TARGET} STREQUAL "Hexagon" AND ${CMAKE_SYSTEM_NAME} STREQUAL "HexagonStandalone")
  MESSAGE(FATAL_ERROR "CMAKE_SYSTEM_NAME is 'HexagonStandalone' but BUILD_TARGET is not 'hexagon'"
          " (BUILD_TARGET = \'${BUILD_TARGET}\'), your build configuration is inconsistent!")
endif()

### Create a test executable, a CTest entry, and an individual test-<target>
### target. `check` builds the executables and runs them through CTest,
### which schedules them with the configured job limit.
### When cross-compiling, tests run under the corresponding emulator
### (see CROSSCOMPILING_EMULATOR).
### The function takes the name of the executable target as the positional
### parameter. The following optional named parameters are:
###   - DEPENDS: list of additional dependencies;
###   - ARGS: list of arguments to pass to the test executable in the command
###           line.
function(add_test_app target)
  cmake_parse_arguments(ARG "" "" "DEPENDS;ARGS" ${ARGN})
  set(TEST_ARGS "${ARG_ARGS}")
  set(FILES ${ARG_UNPARSED_ARGUMENTS})
  separate_arguments(TEST_ARGS)

  # Normally, build each app as an executable, except on QuRT it is shared library.
  if(${BUILD_TARGET} STREQUAL "Hexagon" AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "HexagonStandalone")
    add_library(${target} SHARED ${FILES})
  else()
    add_executable(${target} ${FILES})
  endif()

  # Set some target-specific options automatically.
  if(${BUILD_TARGET} STREQUAL "Hexagon")
    if(${CMAKE_SYSTEM_NAME} STREQUAL "HexagonStandalone")
      MESSAGE(STATUS "Configure \'${target}\' for Hexagon standalone environment")
      target_compile_definitions(${target} PRIVATE "HEXAGON_STANDALONE")
    else()
      MESSAGE(STATUS "Configure \'${target}\' for Hexagon QuRT/FastRPC environment")
      target_compile_options(${target} PRIVATE "-fPIC")
      target_link_libraries(${target} PRIVATE posix)
      target_link_options(${target} PRIVATE LINKER:-Bsymbolic
                          LINKER:--wrap=malloc
                          LINKER:--wrap=calloc
                          LINKER:--wrap=free
                          LINKER:--wrap=realloc
                          LINKER:--wrap=memalign)
    endif()
  endif()

  # CMake resolves the executable target and its configured emulator.
  set(TEST_COMMAND "${target}" --repeat "${RIPPLE_TEST_SUITE_TEST_REPEAT}")
  list(APPEND TEST_COMMAND ${TEST_ARGS})

  add_test(NAME ${target} COMMAND ${TEST_COMMAND} ${FILES})

  set(TEST_TARGET "test-${target}")

  add_custom_target(${TEST_TARGET}
    COMMAND ${TEST_COMMAND} ${FILES}
    VERBATIM
    USES_TERMINAL
    )

  add_dependencies(check ${target})
  add_dependencies(${TEST_TARGET} ${target})

  if (ARG_DEPENDS)
    add_dependencies(check ${ARG_DEPENDS})
    add_dependencies(${TEST_TARGET} ${ARG_DEPENDS})
  endif()
endfunction()


function(link_ispc_shared target ispc_src)
  if (NOT TARGET ${target})
    message(FATAL_ERROR "Executable target '${target}' does not exist.")
  endif()

  # Locate the ISPC compiler
  find_program(ISPC_EXECUTABLE NAMES ispc REQUIRED)
  if (NOT ISPC_EXECUTABLE)
    message(FATAL_ERROR "ISPC compiler not found. Please install ISPC and ensure it is in PATH.")
  endif()

  # Derive names/paths
  get_filename_component(ISPC_SRC_ABS "${ispc_src}" ABSOLUTE)
  get_filename_component(ISPC_BASENAME "${ispc_src}" NAME_WE)
  set(ISPC_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${ISPC_BASENAME}.ispc.o")

  # Compile ISPC to an object for the native host target
  #
  # Notes:
  #   --target=host        : compile for the native target of the build machine
  #   --pic                : generate position-independent code
  #   -O3                  : optimization level
  #
  add_custom_command(
    OUTPUT "${ISPC_OBJ}"
    COMMAND "${ISPC_EXECUTABLE}"
            "${ISPC_SRC_ABS}"
            --target=host
            --pic
            -O3
            -o "${ISPC_OBJ}"
    DEPENDS "${ispc_src}"
    COMMENT "Compiling ISPC source '${ispc_src}' -> '${ISPC_OBJ}'"
    VERBATIM
  )

  # Build a shared library from the ISPC object
  set(ISPC_SHARED_TARGET "${ISPC_BASENAME}_ispc")
  add_library("${ISPC_SHARED_TARGET}" SHARED "${ISPC_OBJ}")
  # Ensure PIC for platforms that honor it
  set_target_properties("${ISPC_SHARED_TARGET}" PROPERTIES POSITION_INDEPENDENT_CODE ON LINKER_LANGUAGE CXX)
  # Link the shared ISPC library to the requested executable
  target_link_libraries("${target}" PRIVATE "${ISPC_SHARED_TARGET}")
endfunction()



# Enable HVX tests for Hexagon.
if(${BUILD_TARGET} STREQUAL "Hexagon")
  add_compile_options("-mhvx")
  set(RIPPLE_TEST_INCLUDE_HVX_RT_LIB ON)
endif()

enable_testing()
add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --build-config "$<CONFIG>"
          --parallel "${RIPPLE_TEST_SUITE_TEST_JOBS}" --verbose
          --output-on-failure
  VERBATIM
  USES_TERMINAL)

set(RIPPLE_TEST_SUITE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(mini_apps)

# regressions
add_subdirectory(regression)
