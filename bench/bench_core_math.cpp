// Copyright 2026 The mpfix Authors
// SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause

// mpfix's double functions (mpfix/double_math.h, the generic functions at 53
// bits) against CORE-MATH's correctly rounded binary64 functions and the C
// library's, in results and in speed.
//
// Results: both mpfix and CORE-MATH claim correct rounding, so their results
// must be the same bits, NaNs aside. The program counts the arguments where
// they differ, on two sets: typical arguments, and arguments of the whole
// range of the function. For the C library it reports the share of results
// that differ from CORE-MATH's and the largest difference in ulps.
//
// Speed: single-threaded throughput, in nanoseconds per call, on the typical
// arguments, as for bench_math.
//
//   bench_core_math [arguments per set, default 100000]

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <vector>

#include "mpfix/double_math.h"

extern "C" {
double cr_exp(double);
double cr_exp2(double);
double cr_exp10(double);
double cr_expm1(double);
double cr_exp2m1(double);
double cr_exp10m1(double);
double cr_log(double);
double cr_log2(double);
double cr_log10(double);
double cr_log1p(double);
double cr_log2p1(double);
double cr_log10p1(double);
double cr_sinh(double);
double cr_cosh(double);
double cr_tanh(double);
double cr_asinh(double);
double cr_acosh(double);
double cr_atanh(double);
double cr_sin(double);
double cr_cos(double);
double cr_tan(double);
double cr_asin(double);
double cr_acos(double);
double cr_atan(double);
double cr_cbrt(double);
double cr_erf(double);
double cr_erfc(double);
double cr_tgamma(double);
double cr_lgamma(double);
double cr_atan2(double, double);
double cr_pow(double, double);
double cr_hypot(double, double);
}

namespace {

struct Rng {
  std::uint64_t state;
  std::uint64_t Next() {
    state += 0x9E3779B97F4A7C15u;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    return z ^ (z >> 31);
  }
  // A uniform double in [0, 1).
  double Unit() { return static_cast<double>(Next() >> 11) * 0x1p-53; }
  bool Bit() { return (Next() & 1) != 0; }
};

// A positive double with a random mantissa and a binary exponent uniform in
// [low, high]. Exponents below -1022 give subnormals.
double Magnitude(Rng& rng, int low, int high) {
  const int span = high - low + 1;
  const int exponent = low + static_cast<int>(rng.Next() % static_cast<std::uint64_t>(span));
  const double mantissa = 1.0 + rng.Unit();
  return std::ldexp(mantissa, exponent);
}

double Signed(Rng& rng, double magnitude) { return rng.Bit() ? -magnitude : magnitude; }

// Generates one argument, from the typical or the wide range of a function.
using Generator = std::function<double(Rng&, bool wide)>;

struct Function {
  const char* name;
  int arity;
  double (*core_math)(double, double);   // Takes y as well; the unary ones ignore it.
  double (*mpfix)(double, double);
  double (*libm)(double, double);        // Or null.
  Generator x;
  Generator y;
};

// Adapters from a unary function to the shared signature, as plain function
// pointers: a lambda without captures converts.
#define UNARY(cr, mp, lm)                                          \
  [](double a, double) { return cr(a); },                          \
      [](double a, double) { return mp(a); },                      \
      lm

// Typical arguments are the ones of bench_math: exponentials and the like in
// (-20, 20), logarithms of 2^-20 to 2^20. The wide range is the whole domain.
Generator ExpRange() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -60, 10) : Magnitude(rng, -20, 4) * 0.6);
  };
}
Generator LogRange() {
  return [](Rng& rng, bool wide) {
    return wide ? Magnitude(rng, -1074, 1023) : Magnitude(rng, -20, 19);
  };
}
Generator AboveOne() {
  return [](Rng& rng, bool wide) {
    return 1.0 + (wide ? Magnitude(rng, -52, 1000) : Magnitude(rng, -20, 19));
  };
}
Generator SmallRange() {  // |x| < 1/4, as for expm1.
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -60, -3) : Magnitude(rng, -10, -3) * 0.5);
  };
}
Generator Log1pRange() {  // x > -1.
  return [](Rng& rng, bool wide) {
    if (rng.Bit()) return wide ? Magnitude(rng, -1074, 1023) : Magnitude(rng, -20, 19);
    return -Magnitude(rng, wide ? -60 : -20, -1);
  };
}
Generator UnitRange() {  // |x| < 1.
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -1074, -1) : Magnitude(rng, -20, -1));
  };
}
Generator TrigRange() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -1074, 1023) : Magnitude(rng, -20, 4) * 0.6);
  };
}
Generator AnyRange() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -1074, 1023) : Magnitude(rng, -20, 19));
  };
}
Generator ErfRange() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -60, 5) : Magnitude(rng, -20, 4) * 0.6);
  };
}
Generator GammaRange() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -60, 8) : Magnitude(rng, -20, 4) * 0.6);
  };
}
Generator PowExponent() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -40, 9) : Magnitude(rng, -20, 4) * 0.6);
  };
}
Generator PowBase() {
  return [](Rng& rng, bool wide) {
    return wide ? Magnitude(rng, -300, 300) : Magnitude(rng, -20, 19);
  };
}

#define MPFIX_FN(name, gen, lm) \
  {#name, 1, UNARY(cr_##name, mpfix::name, lm), gen, nullptr}
#define MPFIX_FN2(name, genx, geny, lm)                                      \
  {#name, 2, [](double a, double b) { return cr_##name(a, b); },             \
   [](double a, double b) { return mpfix::name(a, b); }, lm, genx, geny}

double LibmAtan2(double a, double b) { return std::atan2(a, b); }
double LibmPow(double a, double b) { return std::pow(a, b); }
double LibmHypot(double a, double b) { return std::hypot(a, b); }
#define LIBM1(name) [](double a, double) { return std::name(a); }

std::vector<Function> Functions() {
  return {
      MPFIX_FN(exp, ExpRange(), LIBM1(exp)),
      MPFIX_FN(exp2, ExpRange(), LIBM1(exp2)),
      MPFIX_FN(exp10, ExpRange(), nullptr),
      MPFIX_FN(expm1, SmallRange(), LIBM1(expm1)),
      MPFIX_FN(exp2m1, SmallRange(), nullptr),
      MPFIX_FN(exp10m1, SmallRange(), nullptr),
      MPFIX_FN(log, LogRange(), LIBM1(log)),
      MPFIX_FN(log2, LogRange(), LIBM1(log2)),
      MPFIX_FN(log10, LogRange(), LIBM1(log10)),
      MPFIX_FN(log1p, Log1pRange(), LIBM1(log1p)),
      MPFIX_FN(log2p1, Log1pRange(), nullptr),
      MPFIX_FN(log10p1, Log1pRange(), nullptr),
      MPFIX_FN(sinh, ExpRange(), LIBM1(sinh)),
      MPFIX_FN(cosh, ExpRange(), LIBM1(cosh)),
      MPFIX_FN(tanh, ExpRange(), LIBM1(tanh)),
      MPFIX_FN(asinh, AnyRange(), LIBM1(asinh)),
      MPFIX_FN(acosh, AboveOne(), LIBM1(acosh)),
      MPFIX_FN(atanh, UnitRange(), LIBM1(atanh)),
      MPFIX_FN(sin, TrigRange(), LIBM1(sin)),
      MPFIX_FN(cos, TrigRange(), LIBM1(cos)),
      MPFIX_FN(tan, TrigRange(), LIBM1(tan)),
      MPFIX_FN(asin, UnitRange(), LIBM1(asin)),
      MPFIX_FN(acos, UnitRange(), LIBM1(acos)),
      MPFIX_FN(atan, AnyRange(), LIBM1(atan)),
      MPFIX_FN(cbrt, AnyRange(), LIBM1(cbrt)),
      MPFIX_FN(erf, ErfRange(), LIBM1(erf)),
      MPFIX_FN(erfc, ErfRange(), LIBM1(erfc)),
      MPFIX_FN(tgamma, GammaRange(), LIBM1(tgamma)),
      MPFIX_FN(lgamma, GammaRange(), LIBM1(lgamma)),
      MPFIX_FN2(atan2, AnyRange(), AnyRange(), LibmAtan2),
      MPFIX_FN2(pow, PowBase(), PowExponent(), LibmPow),
      MPFIX_FN2(hypot, AnyRange(), AnyRange(), LibmHypot),
  };
}

// The position of a double in the order of all doubles, as an integer.
std::int64_t Ordinal(double x) {
  const auto bits = std::bit_cast<std::int64_t>(x);
  return bits < 0 ? std::numeric_limits<std::int64_t>::min() - bits : bits;
}

bool SameResult(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
  return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// The difference in ulps, or infinity when one result is not a number or an
// infinity and the other is not the same.
double UlpDistance(double a, double b) {
  if (SameResult(a, b)) return 0;
  if (!std::isfinite(a) || !std::isfinite(b)) return std::numeric_limits<double>::infinity();
  return static_cast<double>(std::abs(Ordinal(a) - Ordinal(b)));
}

struct Precision {
  long mpfix_wrong = 0;
  double first_x = 0, first_y = 0;
  long libm_wrong = 0;
  double libm_max_ulps = 0;
  long count = 0;
};

Precision Check(const Function& f, bool wide, long count, std::uint64_t seed) {
  Rng rng{seed};
  Precision p;
  for (long i = 0; i < count; ++i) {
    const double x = f.x(rng, wide);
    const double y = f.arity == 2 ? f.y(rng, wide) : 0.0;
    const double expected = f.core_math(x, y);
    if (!SameResult(f.mpfix(x, y), expected)) {
      if (p.mpfix_wrong++ == 0) {
        p.first_x = x;
        p.first_y = y;
      }
    }
    if (f.libm != nullptr) {
      const double ulps = UlpDistance(f.libm(x, y), expected);
      if (ulps != 0) ++p.libm_wrong;
      p.libm_max_ulps = std::max(p.libm_max_ulps, ulps);
    }
    ++p.count;
  }
  return p;
}

constexpr int kBlock = 256;
volatile double sink;

template <class Pass>
double BestNanoseconds(Pass pass) {
  using Clock = std::chrono::steady_clock;
  int repeats = 1;
  for (;;) {
    const auto start = Clock::now();
    for (int i = 0; i < repeats; ++i) pass();
    if (Clock::now() - start > std::chrono::milliseconds(20)) break;
    repeats *= 2;
  }
  double best = 1e300;
  for (int round = 0; round < 9; ++round) {
    const auto start = Clock::now();
    for (int i = 0; i < repeats; ++i) pass();
    const std::chrono::duration<double, std::nano> elapsed = Clock::now() - start;
    best = std::min(best, elapsed.count() / (static_cast<double>(repeats) * kBlock));
  }
  return best;
}

double Time(double (*fn)(double, double), const std::vector<double>& xs,
            const std::vector<double>& ys) {
  return BestNanoseconds([&] {
    double sum = 0;
    for (int i = 0; i < kBlock; ++i) sum += fn(xs[static_cast<std::size_t>(i)],
                                               ys[static_cast<std::size_t>(i)]);
    sink = sum;
  });
}

}  // namespace

int main(int argc, char** argv) {
  const long count = argc > 1 ? std::atol(argv[1]) : 100000;
  std::printf("%ld arguments per set and function.\n\n", count);
  std::printf("| function | mpfix differs, typical | mpfix differs, wide range | libm differs, typical | libm max ulps, wide | CORE-MATH ns | mpfix ns | mpfix / CORE-MATH | libm ns |\n");
  std::printf("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n");
  long total_wrong = 0;
  for (const Function& f : Functions()) {
    const Precision typical = Check(f, false, count, 1);
    const Precision wide = Check(f, true, count, 2);
    total_wrong += typical.mpfix_wrong + wide.mpfix_wrong;
    Rng rng{3};
    std::vector<double> xs(kBlock), ys(kBlock, 0.0);
    for (int i = 0; i < kBlock; ++i) {
      xs[static_cast<std::size_t>(i)] = f.x(rng, false);
      if (f.arity == 2) ys[static_cast<std::size_t>(i)] = f.y(rng, false);
    }
    const double cr_ns = Time(f.core_math, xs, ys);
    const double mp_ns = Time(f.mpfix, xs, ys);
    char libm_share[32] = "-", libm_ulps[32] = "-", libm_ns[32] = "-";
    if (f.libm != nullptr) {
      std::snprintf(libm_share, sizeof libm_share, "%.4f%%",
                    100.0 * static_cast<double>(typical.libm_wrong) / static_cast<double>(typical.count));
      std::snprintf(libm_ulps, sizeof libm_ulps, "%g", wide.libm_max_ulps);
      std::snprintf(libm_ns, sizeof libm_ns, "%.1f", Time(f.libm, xs, ys));
    }
    std::printf("| %s | %ld | %ld | %s | %s | %.1f | %.1f | %.0fx | %s |\n", f.name,
                typical.mpfix_wrong, wide.mpfix_wrong, libm_share, libm_ulps, cr_ns, mp_ns,
                mp_ns / cr_ns, libm_ns);
    if (typical.mpfix_wrong + wide.mpfix_wrong > 0) {
      const Precision& bad = typical.mpfix_wrong > 0 ? typical : wide;
      std::printf("|  | first difference at x = %a, y = %a |||||||| \n", bad.first_x, bad.first_y);
    }
    std::fflush(stdout);
  }
  std::printf("\nmpfix differs from CORE-MATH in %ld results.\n", total_wrong);
  return total_wrong == 0 ? 0 : 1;
}
