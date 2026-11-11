// Copyright 2026 The mpfix Authors
// SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause

// The elementary and special functions of mpfix/math.h for double, correctly
// rounded to nearest: mpfix::exp(double) and so on. A double is a
// Float<53, 11>, so these are the results of the generic functions in that
// format, with no code of their own. They are the same bits as CORE-MATH's
// and as MPFR's at 53 bits, but 20 to 100 times slower than CORE-MATH, since
// the generic functions are made for 113 bits and more; see
// bench/bench_core_math.cpp.
//
// Flags and errno are not touched. For flags, call the Float functions with a
// Context.

#ifndef MPFIX_DOUBLE_MATH_H_
#define MPFIX_DOUBLE_MATH_H_

#include "mpfix/float.h"
#include "mpfix/math.h"

namespace mpfix {

// double, as a Float.
using binary64 = Float<53, 11>;

namespace detail {

inline binary64 FromDouble(double x) noexcept { return binary64(x); }
inline double ToDouble(const binary64& x) noexcept {
  return static_cast<double>(x);
}

}  // namespace detail

#define MPFIX_DOUBLE_FUNCTION_1(name)                          \
  inline double name(double x) {                               \
    return detail::ToDouble(name(detail::FromDouble(x)));      \
  }
#define MPFIX_DOUBLE_FUNCTION_2(name)                                      \
  inline double name(double x, double y) {                                 \
    return detail::ToDouble(                                               \
        name(detail::FromDouble(x), detail::FromDouble(y)));               \
  }

MPFIX_DOUBLE_FUNCTION_1(exp)
MPFIX_DOUBLE_FUNCTION_1(exp2)
MPFIX_DOUBLE_FUNCTION_1(exp10)
MPFIX_DOUBLE_FUNCTION_1(expm1)
MPFIX_DOUBLE_FUNCTION_1(exp2m1)
MPFIX_DOUBLE_FUNCTION_1(exp10m1)
MPFIX_DOUBLE_FUNCTION_1(log)
MPFIX_DOUBLE_FUNCTION_1(log2)
MPFIX_DOUBLE_FUNCTION_1(log10)
MPFIX_DOUBLE_FUNCTION_1(log1p)
MPFIX_DOUBLE_FUNCTION_1(log2p1)
MPFIX_DOUBLE_FUNCTION_1(log10p1)
MPFIX_DOUBLE_FUNCTION_1(sinh)
MPFIX_DOUBLE_FUNCTION_1(cosh)
MPFIX_DOUBLE_FUNCTION_1(tanh)
MPFIX_DOUBLE_FUNCTION_1(asinh)
MPFIX_DOUBLE_FUNCTION_1(acosh)
MPFIX_DOUBLE_FUNCTION_1(atanh)
MPFIX_DOUBLE_FUNCTION_1(sin)
MPFIX_DOUBLE_FUNCTION_1(cos)
MPFIX_DOUBLE_FUNCTION_1(tan)
MPFIX_DOUBLE_FUNCTION_1(asin)
MPFIX_DOUBLE_FUNCTION_1(acos)
MPFIX_DOUBLE_FUNCTION_1(atan)
MPFIX_DOUBLE_FUNCTION_1(cbrt)
MPFIX_DOUBLE_FUNCTION_1(erf)
MPFIX_DOUBLE_FUNCTION_1(erfc)
MPFIX_DOUBLE_FUNCTION_1(tgamma)
MPFIX_DOUBLE_FUNCTION_1(lgamma)
MPFIX_DOUBLE_FUNCTION_2(atan2)
MPFIX_DOUBLE_FUNCTION_2(pow)
MPFIX_DOUBLE_FUNCTION_2(hypot)

#undef MPFIX_DOUBLE_FUNCTION_1
#undef MPFIX_DOUBLE_FUNCTION_2

}  // namespace mpfix

#endif  // MPFIX_DOUBLE_MATH_H_
