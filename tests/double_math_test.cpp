// Copyright 2026 The mpfix Authors
// SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause

// The double functions of mpfix/double_math.h: exact results, special values,
// and agreement with the C library to within an ulp or two. Correct rounding
// itself is tested for Float<53, 11> by the oracle tests, and against
// CORE-MATH by bench/bench_core_math.cpp.

#include "mpfix/double_math.h"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();

double Ulps(double a, double b) {
  if (a == b) return 0;
  const auto ordinal = [](double x) {
    const auto bits = std::bit_cast<std::int64_t>(x);
    return bits < 0 ? std::numeric_limits<std::int64_t>::min() - bits : bits;
  };
  return static_cast<double>(std::abs(ordinal(a) - ordinal(b)));
}

TEST(DoubleMath, ExactResults) {
  EXPECT_EQ(mpfix::exp(0.0), 1.0);
  EXPECT_EQ(mpfix::exp2(10.0), 1024.0);
  EXPECT_EQ(mpfix::exp10(3.0), 1000.0);
  EXPECT_EQ(mpfix::log(1.0), 0.0);
  EXPECT_EQ(mpfix::log2(8.0), 3.0);
  EXPECT_EQ(mpfix::log10(1e5), 5.0);
  EXPECT_EQ(mpfix::cbrt(-27.0), -3.0);
  EXPECT_EQ(mpfix::hypot(3.0, 4.0), 5.0);
  EXPECT_EQ(mpfix::pow(9.0, 1.5), 27.0);
  EXPECT_EQ(mpfix::tgamma(5.0), 24.0);
  EXPECT_EQ(mpfix::sin(0.0), 0.0);
  EXPECT_EQ(mpfix::atan2(0.0, 1.0), 0.0);
  EXPECT_TRUE(std::signbit(mpfix::sin(-0.0)));
}

TEST(DoubleMath, SpecialValues) {
  EXPECT_EQ(mpfix::exp(kInf), kInf);
  EXPECT_EQ(mpfix::exp(-kInf), 0.0);
  EXPECT_EQ(mpfix::exp(1000.0), kInf);
  EXPECT_TRUE(std::isnan(mpfix::log(-1.0)));
  EXPECT_EQ(mpfix::log(0.0), -kInf);
  EXPECT_TRUE(std::isnan(mpfix::sin(kInf)));
  EXPECT_TRUE(std::isnan(mpfix::exp(kNaN)));
  EXPECT_TRUE(std::isnan(mpfix::asin(2.0)));
  EXPECT_EQ(mpfix::atan(kInf), std::atan(kInf));
  EXPECT_EQ(mpfix::tanh(kInf), 1.0);
  EXPECT_EQ(mpfix::pow(0.0, -1.0), kInf);
  EXPECT_EQ(mpfix::pow(kNaN, 0.0), 1.0);
  EXPECT_EQ(mpfix::lgamma(1.0), 0.0);
}

TEST(DoubleMath, AgreesWithTheCLibrary) {
  // Within 2 ulps: the C library is not always correctly rounded.
  for (double x : {0.001, 0.3, 0.7, 1.0, 1.5, 2.5, 10.0, 33.3}) {
    EXPECT_LE(Ulps(mpfix::exp(x * 0.1), std::exp(x * 0.1)), 2) << x;
    EXPECT_LE(Ulps(mpfix::log(x), std::log(x)), 2) << x;
    EXPECT_LE(Ulps(mpfix::sin(x), std::sin(x)), 2) << x;
    EXPECT_LE(Ulps(mpfix::cos(x), std::cos(x)), 2) << x;
    EXPECT_LE(Ulps(mpfix::tan(x), std::tan(x)), 2) << x;
    EXPECT_LE(Ulps(mpfix::atan(x), std::atan(x)), 2) << x;
    EXPECT_LE(Ulps(mpfix::sinh(x * 0.1), std::sinh(x * 0.1)), 2) << x;
    EXPECT_LE(Ulps(mpfix::cbrt(x), std::cbrt(x)), 2) << x;
    EXPECT_LE(Ulps(mpfix::erf(x * 0.1), std::erf(x * 0.1)), 2) << x;
    EXPECT_LE(Ulps(mpfix::pow(x, 0.3), std::pow(x, 0.3)), 2) << x;
    EXPECT_LE(Ulps(mpfix::atan2(x, 0.4), std::atan2(x, 0.4)), 2) << x;
  }
}

TEST(DoubleMath, SameAsTheFloatFunction) {
  using F = mpfix::binary64;
  for (double x : {0.1, 0.9, 3.7, 12.25}) {
    EXPECT_EQ(mpfix::exp(x), static_cast<double>(exp(F(x))));
    EXPECT_EQ(mpfix::log1p(x), static_cast<double>(log1p(F(x))));
    EXPECT_EQ(mpfix::lgamma(x), static_cast<double>(lgamma(F(x))));
    EXPECT_EQ(mpfix::hypot(x, 2.0), static_cast<double>(hypot(F(x), F(2.0))));
  }
}

}  // namespace
