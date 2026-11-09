// Prints Floats as "P E <bytes in hex> <text>" for debug/test_mpfix_format.py,
// which decodes the bytes the way the debugger scripts do and compares the
// result with the library's own text.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>

#include "mpfix/charconv.h"
#include "mpfix/float.h"

namespace {

template <int P, int E>
void Print(const mpfix::Float<P, E>& x) {
  unsigned char bytes[sizeof(x)];
  std::memcpy(bytes, &x, sizeof(x));
  std::string hex;
  for (const unsigned char byte : bytes) {
    char pair[3];
    std::snprintf(pair, sizeof(pair), "%02x", static_cast<unsigned>(byte));
    hex += pair;
  }
  std::printf("%d %d %s %s\n", P, E, hex.c_str(), to_string(x).c_str());
}

template <int P, int E>
void Dump(std::mt19937_64& random, int random_count) {
  using F = mpfix::Float<P, E>;
  Print(F::Zero(false));
  Print(F::Zero(true));
  Print(F::Infinity(false));
  Print(F::Infinity(true));
  Print(F::QuietNaN());
  Print(F(1));
  Print(F(-1));
  Print(F(3));
  Print(F(0.1));
  Print(F(1) / F(3));
  Print((std::numeric_limits<F>::max)());
  Print(-(std::numeric_limits<F>::max)());
  Print(F::MinNormal());
  Print(F::MinSubnormal());
  Print(F::MinSubnormal() * F(3));
  Print(F::MinNormal() - F::MinSubnormal());
  F x(1);
  for (int i = 0; i < 40; ++i) {
    Print(x);
    x = x * F(7);
  }
  x = F(1);
  for (int i = 0; i < 40; ++i) {
    Print(x);
    x = x / F(10);
  }
  for (int i = 0; i < random_count; ++i) {
    typename F::Bits bits;
    for (auto& limb : bits) limb = random();
    const F value = F::from_bits(bits);
    Print(value);
    // Values near a power of two, where the spacing changes.
    Print(next_up(value));
  }
}

}  // namespace

int main() {
  std::mt19937_64 random(12345);
  Dump<11, 5>(random, 200);
  Dump<24, 8>(random, 200);
  Dump<53, 11>(random, 200);
  Dump<64, 15>(random, 100);
  Dump<113, 15>(random, 200);
  Dump<237, 19>(random, 100);
  Dump<489, 23>(random, 100);
  return 0;
}
