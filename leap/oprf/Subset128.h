#ifndef SUBSET128_H
#define SUBSET128_H

#include <gmpxx.h>
#include <stdint.h>
#include <vector>
//#ifdef __AVX__
// avx2
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <immintrin.h>
#include <stdexcept>
#include <x86intrin.h>

//#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Crypto/PRNG.h>

extern "C" {
#include <leap/oprf/params.h>
#include <leap/oprf/precomputations.h>
}
#include <leap/thirdparty/SHAKE256/SHAKE256.h>

using namespace std;

namespace LEAP {
class Subset128 {
public:
  // new subset
  Subset128();
  Subset128(uint8_t input[]);
  Subset128(uint16_t input[]);
  Subset128(osuCrypto::block b, SHAKE256 *shake256);
  ~Subset128();
  Subset128(const Subset128 &poly);

  uint8_t get(size_t idx);
  void get_full(uint8_t *arr);
  void rand_poly(osuCrypto::PRNG *_prng);
  void print_poly() const;
  void get_int_repr(uint8_t *out);
  void get_int_from_subsetsum(uint16_t *out);

  // Operators
  friend Subset128 operator+(const Subset128 &a, const Subset128 &b);
  friend Subset128 operator-(const Subset128 &a, const Subset128 &b);
  friend Subset128 operator^(const Subset128 &a, const Subset128 &b);
  friend bool operator==(const Subset128 &a, const Subset128 &b);
  Subset128 &operator=(const Subset128 &b) {
    for (size_t i = 0; i < 4; ++i)
      dlog[i] = b.dlog[i];
    return *this;
  }

  // discrete log representation
  std::array<__m256i, 4> dlog;
};

} // namespace LEAP
#endif
