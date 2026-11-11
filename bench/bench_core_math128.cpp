// Copyright 2026 The mpfix Authors
// SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause

// mpfix's binary128 functions (mpfix/math.h) against CORE-MATH's binary128
// functions and libquadmath's, in results and in speed, for the functions that
// CORE-MATH has in binary128: exp, exp2, exp10, expm1, log, sin, cos, tan,
// asin, acos, atan, atan2, cbrt and hypot. Needs GCC, for __float128, and
// MPFR.
//
// Results: every result is compared with MPFR's, correctly rounded to
// nearest, on typical arguments and on arguments of the whole range of the
// function. The program counts the results that differ, for each library.
// MPFR is the reference, and not CORE-MATH, since a library can be wrong for
// the platform it is built on: CORE-MATH's sources assume that long is 64 bits,
// which it is not on Windows.
//
// Speed: single-threaded throughput, in nanoseconds per call, on the typical
// arguments.
//
//   bench_core_math128 [arguments per set, default 20000]

#include <gmp.h>
#include <mpfr.h>
#include <quadmath.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "mpfix/math.h"

__extension__ typedef __float128 Q;
__extension__ typedef unsigned __int128 U128;

extern "C" {
Q cr_expq(Q);
Q cr_exp2q(Q);
Q cr_exp10q(Q);
Q cr_expm1q(Q);
Q cr_logq(Q);
Q cr_sinq(Q);
Q cr_cosq(Q);
Q cr_tanq(Q);
Q cr_asinq(Q);
Q cr_acosq(Q);
Q cr_atanq(Q);
Q cr_atan2q(Q, Q);
Q cr_cbrtq(Q);
Q cr_hypotq(Q, Q);
}

namespace {

using F = mpfix::binary128;

// __float128 and binary128 have the same bits.
Q ToQ(const F& x) {
  const F::Bits bits = x.to_bits();
  Q q;
  std::memcpy(&q, bits.data(), sizeof q);
  return q;
}
F ToF(Q q) {
  F::Bits bits;
  std::memcpy(bits.data(), &q, sizeof q);
  return F::from_bits(bits);
}
U128 BitsOf(Q q) {
  U128 bits;
  std::memcpy(&bits, &q, sizeof bits);
  return bits;
}
Q FromBits(U128 bits) {
  Q q;
  std::memcpy(&q, &bits, sizeof q);
  return q;
}

struct Rng {
  std::uint64_t state;
  std::uint64_t Next() {
    state += 0x9E3779B97F4A7C15u;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    return z ^ (z >> 31);
  }
  bool Bit() { return (Next() & 1) != 0; }
};

// A positive binary128 with a random 113-bit significand and a binary exponent
// uniform in [low, high]. Exponents below -16382 give subnormals, with fewer
// random bits.
Q Magnitude(Rng& rng, int low, int high) {
  const int span = high - low + 1;
  const int exponent =
      low + static_cast<int>(rng.Next() % static_cast<std::uint64_t>(span));
  U128 fraction = (static_cast<U128>(rng.Next()) << 64 | rng.Next()) &
                  ((static_cast<U128>(1) << 112) - 1);
  U128 biased = static_cast<U128>(exponent + 16383);
  if (exponent < -16382) {
    // Subnormal: shift the implicit one into the fraction.
    fraction = (fraction | static_cast<U128>(1) << 112) >> (-16382 - exponent);
    biased = 0;
  }
  return FromBits(biased << 112 | fraction);
}

Q Signed(Rng& rng, Q magnitude) { return rng.Bit() ? -magnitude : magnitude; }

using Generator = std::function<Q(Rng&, bool wide)>;

using Mpfr1 = int (*)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
using Mpfr2 = int (*)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);

struct Function {
  const char* name;
  int arity;
  Q (*core_math)(Q, Q);  // The unary ones ignore y.
  Q (*mpfix)(Q, Q);
  Q (*quadmath)(Q, Q);   // Or null.
  Mpfr1 mpfr1;
  Mpfr2 mpfr2;
  Generator x;
  Generator y;
};

// Typical arguments are as for bench_math: in magnitude from 2^-20 to 2^4 for
// the exponentials, 2^-20 to 2^20 for the logarithm. The wide range is the
// whole domain of the function.
Generator ExpRange() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -120, 13) : Magnitude(rng, -20, 4));
  };
}
Generator SmallRange() {  // |x| < 1/4, as for expm1.
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -120, -3) : Magnitude(rng, -10, -3));
  };
}
Generator LogRange() {
  return [](Rng& rng, bool wide) {
    return wide ? Magnitude(rng, -16494, 16383) : Magnitude(rng, -20, 19);
  };
}
Generator UnitRange() {  // |x| < 1.
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -16494, -1) : Magnitude(rng, -20, -1));
  };
}
Generator TrigRange() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -16494, 16383) : Magnitude(rng, -20, 4));
  };
}
Generator AnyRange() {
  return [](Rng& rng, bool wide) {
    return Signed(rng, wide ? Magnitude(rng, -16494, 16383) : Magnitude(rng, -20, 19));
  };
}

#define MPFIX_FN(name, qname, gen)                                        \
  {#name, 1, [](Q a, Q) { return cr_##name##q(a); },                      \
   [](Q a, Q) { return ToQ(mpfix::name(ToF(a))); },                       \
   [](Q a, Q) { return qname##q(a); }, mpfr_##name, nullptr, gen, nullptr}
// libquadmath has no exp10q.
#define MPFIX_FN_NO_QUADMATH(name, gen)                                   \
  {#name, 1, [](Q a, Q) { return cr_##name##q(a); },                      \
   [](Q a, Q) { return ToQ(mpfix::name(ToF(a))); }, nullptr,              \
   mpfr_##name, nullptr, gen, nullptr}
#define MPFIX_FN2(name, genx, geny)                                       \
  {#name, 2, [](Q a, Q b) { return cr_##name##q(a, b); },                 \
   [](Q a, Q b) { return ToQ(mpfix::name(ToF(a), ToF(b))); },             \
   [](Q a, Q b) { return name##q(a, b); }, nullptr, mpfr_##name, genx,    \
   geny}

std::vector<Function> Functions() {
  return {
      MPFIX_FN(exp, exp, ExpRange()),
      MPFIX_FN(exp2, exp2, ExpRange()),
      MPFIX_FN_NO_QUADMATH(exp10, ExpRange()),
      MPFIX_FN(expm1, expm1, SmallRange()),
      MPFIX_FN(log, log, LogRange()),
      MPFIX_FN(sin, sin, TrigRange()),
      MPFIX_FN(cos, cos, TrigRange()),
      MPFIX_FN(tan, tan, TrigRange()),
      MPFIX_FN(asin, asin, UnitRange()),
      MPFIX_FN(acos, acos, UnitRange()),
      MPFIX_FN(atan, atan, AnyRange()),
      MPFIX_FN(cbrt, cbrt, AnyRange()),
      MPFIX_FN2(atan2, AnyRange(), AnyRange()),
      MPFIX_FN2(hypot, AnyRange(), AnyRange()),
  };
}

// --- MPFR as the reference ---

// A binary128 in an mpfr_t, exactly, which needs a precision of 128 or more.
void SetMpfr(mpfr_ptr r, Q q) {
  const U128 bits = BitsOf(q);
  const bool negative = (bits >> 127) != 0;
  const int biased = static_cast<int>((bits >> 112) & 0x7fff);
  U128 fraction = bits & ((static_cast<U128>(1) << 112) - 1);
  if (biased == 0x7fff) {
    if (fraction != 0) {
      mpfr_set_nan(r);
    } else {
      mpfr_set_inf(r, negative ? -1 : 1);
    }
    return;
  }
  int exponent = biased - 16383 - 112;
  if (biased == 0) {
    exponent = 1 - 16383 - 112;
  } else {
    fraction |= static_cast<U128>(1) << 112;
  }
  mpfr_set_ui(r, 0, MPFR_RNDN);
  for (int word = 3; word >= 0; --word) {
    mpfr_mul_2ui(r, r, 32, MPFR_RNDN);
    mpfr_add_ui(r, r, static_cast<std::uint32_t>(fraction >> (32 * word)), MPFR_RNDN);
  }
  mpfr_mul_2si(r, r, exponent, MPFR_RNDN);
  if (negative) mpfr_neg(r, r, MPFR_RNDN);
}

// The correctly rounded result at binary128's precision and exponent range,
// subnormals included.
void Reference(const Function& f, Q x, Q y, mpfr_ptr result) {
  mpfr_t a, b;
  mpfr_init2(a, 128);
  mpfr_init2(b, 128);
  SetMpfr(a, x);
  SetMpfr(b, y);
  mpfr_set_prec(result, 113);
  const int ternary = f.arity == 1 ? f.mpfr1(result, a, MPFR_RNDN)
                                   : f.mpfr2(result, a, b, MPFR_RNDN);
  mpfr_subnormalize(result, ternary, MPFR_RNDN);
  mpfr_clear(a);
  mpfr_clear(b);
}

bool Matches(Q candidate, mpfr_srcptr reference) {
  mpfr_t c;
  mpfr_init2(c, 128);
  SetMpfr(c, candidate);
  bool same;
  if (mpfr_nan_p(reference) || mpfr_nan_p(c)) {
    same = mpfr_nan_p(reference) && mpfr_nan_p(c);
  } else {
    // Equal, and the same sign of zero.
    same = mpfr_equal_p(c, reference) && mpfr_signbit(c) == mpfr_signbit(reference);
  }
  mpfr_clear(c);
  return same;
}

struct Wrong {
  long count = 0;
  Q first_x = 0, first_y = 0;
  void Add(Q x, Q y) {
    if (count++ == 0) {
      first_x = x;
      first_y = y;
    }
  }
};

struct Precision {
  Wrong mpfix, core_math, quadmath;
};

Precision Check(const Function& f, bool wide, long count, std::uint64_t seed) {
  Rng rng{seed};
  Precision p;
  mpfr_t reference;
  mpfr_init2(reference, 113);
  for (long i = 0; i < count; ++i) {
    const Q x = f.x(rng, wide);
    const Q y = f.arity == 2 ? f.y(rng, wide) : Q(0);
    Reference(f, x, y, reference);
    if (!Matches(f.mpfix(x, y), reference)) p.mpfix.Add(x, y);
    if (!Matches(f.core_math(x, y), reference)) p.core_math.Add(x, y);
    if (f.quadmath != nullptr && !Matches(f.quadmath(x, y), reference)) {
      p.quadmath.Add(x, y);
    }
  }
  mpfr_clear(reference);
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

double Time(Q (*fn)(Q, Q), const std::vector<Q>& xs, const std::vector<Q>& ys) {
  return BestNanoseconds([&] {
    U128 mix = 0;
    for (int i = 0; i < kBlock; ++i) {
      mix ^= BitsOf(fn(xs[static_cast<std::size_t>(i)], ys[static_cast<std::size_t>(i)]));
    }
    sink = static_cast<double>(static_cast<std::uint64_t>(mix));
  });
}

std::string Hex(Q x) {
  char text[64];
  std::snprintf(text, sizeof text, "%016llx%016llx",
                static_cast<unsigned long long>(BitsOf(x) >> 64),
                static_cast<unsigned long long>(BitsOf(x)));
  return text;
}

void Cell(char (&text)[32], const Wrong& typical, const Wrong& wide) {
  std::snprintf(text, sizeof text, "%ld / %ld", typical.count, wide.count);
}

}  // namespace

int main(int argc, char** argv) {
  const long count = argc > 1 ? std::atol(argv[1]) : 20000;
  mpfr_set_emin(-16493);  // binary128, with its subnormals.
  mpfr_set_emax(16384);
  std::printf("%ld typical and %ld wide-range arguments per function. A cell is "
              "the number of results that differ from MPFR's, on typical / "
              "wide-range arguments.\n\n", count, count);
  std::printf("| function | mpfix wrong | CORE-MATH wrong | libquadmath wrong | CORE-MATH ns | mpfix ns | mpfix / CORE-MATH | libquadmath ns |\n");
  std::printf("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n");
  long mpfix_wrong = 0;
  for (const Function& f : Functions()) {
    const Precision typical = Check(f, false, count, 1);
    const Precision wide = Check(f, true, count, 2);
    mpfix_wrong += typical.mpfix.count + wide.mpfix.count;
    Rng rng{3};
    std::vector<Q> xs(kBlock), ys(kBlock, Q(0));
    for (int i = 0; i < kBlock; ++i) {
      xs[static_cast<std::size_t>(i)] = f.x(rng, false);
      if (f.arity == 2) ys[static_cast<std::size_t>(i)] = f.y(rng, false);
    }
    const double cr_ns = Time(f.core_math, xs, ys);
    const double mp_ns = Time(f.mpfix, xs, ys);
    char mp_cell[32], cr_cell[32], q_cell[32] = "-", q_time[32] = "-";
    Cell(mp_cell, typical.mpfix, wide.mpfix);
    Cell(cr_cell, typical.core_math, wide.core_math);
    if (f.quadmath != nullptr) {
      Cell(q_cell, typical.quadmath, wide.quadmath);
      std::snprintf(q_time, sizeof q_time, "%.1f", Time(f.quadmath, xs, ys));
    }
    std::printf("| %s | %s | %s | %s | %.1f | %.1f | %.1fx | %s |\n", f.name, mp_cell,
                cr_cell, q_cell, cr_ns, mp_ns, mp_ns / cr_ns, q_time);
    const struct {
      const char* who;
      const Wrong* wrong[2];
    } reports[] = {{"mpfix", {&typical.mpfix, &wide.mpfix}},
                   {"CORE-MATH", {&typical.core_math, &wide.core_math}}};
    for (const auto& report : reports) {
      for (const Wrong* wrong : report.wrong) {
        if (wrong->count > 0) {
          std::printf("|  | first %s difference: x = 0x%s, y = 0x%s |||||||\n", report.who,
                      Hex(wrong->first_x).c_str(), Hex(wrong->first_y).c_str());
          break;
        }
      }
    }
    std::fflush(stdout);
  }
  std::printf("\nmpfix differs from MPFR in %ld results.\n", mpfix_wrong);
  return mpfix_wrong == 0 ? 0 : 1;
}
