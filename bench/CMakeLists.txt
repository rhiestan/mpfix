# Benchmarks compare mpfix with MPFR and Boost.Multiprecision. Boost is
# header-only here, so only its include directory is needed.
get_filename_component(mpfix_parent_dir "${PROJECT_SOURCE_DIR}" DIRECTORY)
set(MPFIX_BOOST_DIR "${mpfix_parent_dir}/boost_1_92_0" CACHE PATH
    "Boost source tree used by the benchmarks")
if(NOT EXISTS "${MPFIX_BOOST_DIR}/boost/multiprecision/cpp_bin_float.hpp")
  message(FATAL_ERROR "Boost not found; set MPFIX_BOOST_DIR")
endif()

find_path(MPFIX_GMP_INCLUDE_DIR gmp.h REQUIRED)
find_path(MPFIX_MPFR_INCLUDE_DIR mpfr.h REQUIRED)
find_library(MPFIX_GMP_LIBRARY NAMES gmp libgmp REQUIRED)
find_library(MPFIX_MPFR_LIBRARY NAMES mpfr libmpfr REQUIRED)

add_executable(bench_scalar bench_scalar.cpp)
mpfix_warnings(bench_scalar)
target_link_libraries(bench_scalar PRIVATE mpfix
  "${MPFIX_MPFR_LIBRARY}" "${MPFIX_GMP_LIBRARY}")
target_include_directories(bench_scalar SYSTEM PRIVATE
  "${MPFIX_BOOST_DIR}" "${MPFIX_GMP_INCLUDE_DIR}" "${MPFIX_MPFR_INCLUDE_DIR}")

# Berkeley SoftFloat 3e, optionally, for binary128: a tree with
# source/include/softfloat.h and the library built from it, as a static library
# or, on Windows, a DLL with its import library in dll/.
set(MPFIX_SOFTFLOAT_DIR "${mpfix_parent_dir}/gmp-build/softfloat" CACHE PATH
    "Berkeley SoftFloat tree for the scalar benchmark, if it exists")
find_library(MPFIX_SOFTFLOAT_LIBRARY NAMES softfloat libsoftfloat softfloat.dll
  PATHS "${MPFIX_SOFTFLOAT_DIR}/dll" "${MPFIX_SOFTFLOAT_DIR}/build"
  NO_DEFAULT_PATH)
if(MPFIX_SOFTFLOAT_LIBRARY AND
   EXISTS "${MPFIX_SOFTFLOAT_DIR}/source/include/softfloat.h")
  target_include_directories(bench_scalar SYSTEM PRIVATE
    "${MPFIX_SOFTFLOAT_DIR}/source/include")
  target_link_libraries(bench_scalar PRIVATE "${MPFIX_SOFTFLOAT_LIBRARY}")
  # SoftFloat's default build, whose binary128 functions take values.
  target_compile_definitions(bench_scalar PRIVATE MPFIX_BENCH_SOFTFLOAT
    SOFTFLOAT_FAST_INT64
    $<$<BOOL:${WIN32}>:SOFTFLOAT_DLL>)
  message(STATUS "Benchmarking SoftFloat: ${MPFIX_SOFTFLOAT_LIBRARY}")
endif()
target_compile_options(bench_scalar PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU,AppleClang>:-march=native>)

add_executable(bench_strings bench_strings.cpp)
mpfix_warnings(bench_strings)
target_link_libraries(bench_strings PRIVATE mpfix
  "${MPFIX_MPFR_LIBRARY}" "${MPFIX_GMP_LIBRARY}")
target_include_directories(bench_strings SYSTEM PRIVATE
  "${MPFIX_BOOST_DIR}" "${MPFIX_GMP_INCLUDE_DIR}" "${MPFIX_MPFR_INCLUDE_DIR}")
target_compile_options(bench_strings PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU,AppleClang>:-march=native>)

add_executable(bench_math bench_math.cpp)
mpfix_warnings(bench_math)
target_link_libraries(bench_math PRIVATE mpfix
  "${MPFIX_MPFR_LIBRARY}" "${MPFIX_GMP_LIBRARY}")
target_include_directories(bench_math SYSTEM PRIVATE
  "${MPFIX_BOOST_DIR}" "${MPFIX_GMP_INCLUDE_DIR}" "${MPFIX_MPFR_INCLUDE_DIR}")
target_compile_options(bench_math PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU,AppleClang>:-march=native>)

# The batch prototype runs on Google Highway's vectors.
FetchContent_Declare(highway
  GIT_REPOSITORY https://github.com/google/highway.git
  GIT_TAG ${MPFIX_HIGHWAY_TAG}
  SYSTEM
  GIT_SHALLOW TRUE)
set(HWY_ENABLE_CONTRIB OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(highway)

add_executable(bench_batch bench_batch.cpp)
mpfix_warnings(bench_batch)
target_link_libraries(bench_batch PRIVATE mpfix hwy)
target_compile_options(bench_batch PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU,AppleClang>:-march=native>)

# CORE-MATH's correctly rounded binary64 functions, if its tree exists and the C
# compiler is GCC or Clang (the sources use __int128): the double benchmark
# compares mpfix's double functions with them. The tree is used as it is, with
# the flags of its own scripts.
set(MPFIX_CORE_MATH_DIR "${mpfix_parent_dir}/core-math-master" CACHE PATH
    "CORE-MATH source tree for the double benchmark, if it exists")
if(EXISTS "${MPFIX_CORE_MATH_DIR}/src/binary64/exp/exp.c" AND
   CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  set(core_math_sources "${CMAKE_CURRENT_SOURCE_DIR}/core_math_support.c")
  foreach(function exp exp2 exp10 expm1 exp2m1 exp10m1 log log2 log10 log1p
          log2p1 log10p1 sinh cosh tanh asinh acosh atanh sin cos tan asin acos
          atan atan2 pow cbrt hypot erf erfc tgamma lgamma)
    list(APPEND core_math_sources
         "${MPFIX_CORE_MATH_DIR}/src/binary64/${function}/${function}.c")
  endforeach()
  add_library(core_math_binary64 STATIC ${core_math_sources})
  target_compile_options(core_math_binary64 PRIVATE -O3 -march=native -w)
  set_source_files_properties(
    "${MPFIX_CORE_MATH_DIR}/src/binary64/lgamma/lgamma.c" PROPERTIES
    COMPILE_DEFINITIONS signgam=mpfix_bench_signgam
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/core_math_support.h")

  add_executable(bench_core_math bench_core_math.cpp)
  mpfix_warnings(bench_core_math)
  target_link_libraries(bench_core_math PRIVATE mpfix core_math_binary64)
  target_compile_options(bench_core_math PRIVATE
    $<$<CXX_COMPILER_ID:Clang,GNU,AppleClang>:-march=native>)
  message(STATUS "Benchmarking against CORE-MATH: ${MPFIX_CORE_MATH_DIR}")
endif()

# CORE-MATH's binary128 functions, with GCC, for __float128: the same
# comparison for mpfix's binary128, against CORE-MATH and libquadmath.
if(EXISTS "${MPFIX_CORE_MATH_DIR}/src/binary128/exp/expq.c" AND
   CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(core_math_128_sources)
  foreach(function exp/expq exp2/exp2q exp10/exp10q expm1/expm1q log/logq
          sin/sinq cos/cosq tan/tanq asin/asinq acos/acosq atan/atanq
          atan2/atan2q cbrt/cbrtq hypot/hypotq)
    list(APPEND core_math_128_sources
         "${MPFIX_CORE_MATH_DIR}/src/binary128/${function}.c")
  endforeach()
  add_library(core_math_binary128 STATIC ${core_math_128_sources})
  target_compile_options(core_math_binary128 PRIVATE -O3 -march=native -w)
  # Each file also defines a plain expq and the like, which forwards to the
  # C library's expf128, for use as a libm replacement. That would clash with
  # libquadmath's, and MinGW has no expf128. On Windows, __APPLE__ skips them
  # and changes nothing else, since the other uses of it are with _WIN32. On
  # other systems the functions are renamed away; untested.
  if(WIN32)
    target_compile_definitions(core_math_binary128 PRIVATE __APPLE__)
  else()
    foreach(function exp exp2 exp10 expm1 log sin cos tan asin acos atan atan2
            cbrt hypot)
      target_compile_definitions(core_math_binary128 PRIVATE
        ${function}q=mpfix_bench_unused_${function}q)
    endforeach()
  endif()

  add_executable(bench_core_math128 bench_core_math128.cpp)
  mpfix_warnings(bench_core_math128)
  target_link_libraries(bench_core_math128 PRIVATE mpfix core_math_binary128
                        quadmath "${MPFIX_MPFR_LIBRARY}" "${MPFIX_GMP_LIBRARY}")
  target_include_directories(bench_core_math128 SYSTEM PRIVATE
    "${MPFIX_GMP_INCLUDE_DIR}" "${MPFIX_MPFR_INCLUDE_DIR}")
  target_compile_options(bench_core_math128 PRIVATE -march=native)
  message(STATUS "Benchmarking binary128 against CORE-MATH")
endif()
