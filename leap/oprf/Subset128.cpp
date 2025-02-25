#include <leap/oprf/Subset128.h>

namespace LEAP {

////////////////////////////////////////////////////
/////////////// Constructors ///////////////////////
////////////////////////////////////////////////////

Subset128::Subset128() : dlog() {
  for (size_t i = 0; i < 4; ++i) {
    this->dlog[i] = _mm256_setzero_si256();
  }
}

Subset128::Subset128(const Subset128 &poly) {
  for (auto i = 0; i < 4; ++i)
    this->dlog[i] = poly.dlog[i];
}
Subset128::Subset128(uint16_t input[]) {
  uint8_t sum[N];
  for (auto i = 0; i < N; ++i) {
    sum[i] = (uint8_t)inversePowers[input[i]];
  }
  for (auto i = 0; i < 4; ++i)
    dlog[i] = _mm256_setr_epi8(
        sum[32 * i + 0], sum[32 * i + 1], sum[32 * i + 2], sum[32 * i + 3],
        sum[32 * i + 4], sum[32 * i + 5], sum[32 * i + 6], sum[32 * i + 7],
        sum[32 * i + 8], sum[32 * i + 9], sum[32 * i + 10], sum[32 * i + 11],
        sum[32 * i + 12], sum[32 * i + 13], sum[32 * i + 14], sum[32 * i + 15],
        sum[32 * i + 16], sum[32 * i + 17], sum[32 * i + 18], sum[32 * i + 19],
        sum[32 * i + 20], sum[32 * i + 21], sum[32 * i + 22], sum[32 * i + 23],
        sum[32 * i + 24], sum[32 * i + 25], sum[32 * i + 26], sum[32 * i + 27],
        sum[32 * i + 28], sum[32 * i + 29], sum[32 * i + 30], sum[32 * i + 31]);
}

Subset128::Subset128(uint8_t input[]) {
  for (auto i = 0; i < 4; ++i)
    dlog[i] = _mm256_setr_epi8(
        input[32 * i + 0], input[32 * i + 1], input[32 * i + 2],
        input[32 * i + 3], input[32 * i + 4], input[32 * i + 5],
        input[32 * i + 6], input[32 * i + 7], input[32 * i + 8],
        input[32 * i + 9], input[32 * i + 10], input[32 * i + 11],
        input[32 * i + 12], input[32 * i + 13], input[32 * i + 14],
        input[32 * i + 15], input[32 * i + 16], input[32 * i + 17],
        input[32 * i + 18], input[32 * i + 19], input[32 * i + 20],
        input[32 * i + 21], input[32 * i + 22], input[32 * i + 23],
        input[32 * i + 24], input[32 * i + 25], input[32 * i + 26],
        input[32 * i + 27], input[32 * i + 28], input[32 * i + 29],
        input[32 * i + 30], input[32 * i + 31]);
}
Subset128::Subset128(const osuCrypto::block b, SHAKE256 *shake256) {
  shake256->Reset(1);
  shake256->Update((uint8_t *)&b, 128 / 8);
  for (auto i = 0; i < 4; ++i) {
    uint8_t input[32];
    for (auto j = 0; j < 32; ++j) {
      shake256->Final(&input[j]);
    }
    // TODO change to setr_epi64?
    dlog[i] = _mm256_setr_epi8(
        input[0], input[1], input[2], input[3], input[4], input[5], input[6],
        input[7], input[8], input[9], input[10], input[11], input[12],
        input[13], input[14], input[15], input[16], input[17], input[18],
        input[19], input[20], input[21], input[22], input[23], input[24],
        input[25], input[26], input[27], input[28], input[29], input[30],
        input[31]);
  }
}

void Subset128::rand_poly(osuCrypto::PRNG *_prng) {
  for (auto i = 0; i < 4; ++i) {
    uint8_t input[32];
    for (auto j = 0; j < 32; ++j) {
      input[j] = _prng->get<uint8_t>();
    }
    dlog[i] = _mm256_setr_epi8(
        input[0], input[1], input[2], input[3], input[4], input[5], input[6],
        input[7], input[8], input[9], input[10], input[11], input[12],
        input[13], input[14], input[15], input[16], input[17], input[18],
        input[19], input[20], input[21], input[22], input[23], input[24],
        input[25], input[26], input[27], input[28], input[29], input[30],
        input[31]);
  }
}

Subset128::~Subset128() {}

////////////////////////////////////////////////////
////////////////// Operators ///////////////////////
////////////////////////////////////////////////////

bool operator==(const Subset128 &a, const Subset128 &b) {
  __m256i res;
  for (size_t i = 0; i < 4; ++i) {
    res = _mm256_cmpeq_epi8(a.dlog[i], b.dlog[i]);
    uint64_t *arr = (uint64_t *)&res;
    for (size_t j = 0; j < 4; ++j) {
      if (arr[j] != UINT64_MAX) {
        return false;
      }
    }
  }
  return true;
}

Subset128 operator+(const Subset128 &a, const Subset128 &b) {
  Subset128 ret;
  for (size_t i = 0; i < 4; ++i) {
    ret.dlog[i] = _mm256_add_epi8(a.dlog[i], b.dlog[i]);
  }
  return ret;
}

Subset128 operator-(const Subset128 &a, const Subset128 &b) {
  Subset128 ret;
  for (size_t i = 0; i < 4; ++i) {
    ret.dlog[i] = _mm256_sub_epi8(a.dlog[i], b.dlog[i]);
  }
  return ret;
}

Subset128 inv(const Subset128 &a) {
  Subset128 ret;
  for (size_t i = 0; i < N; ++i) {
    ret.dlog[i] = _mm256_sub_epi8(ret.dlog[i], a.dlog[i]);
  }
  return ret;
}

Subset128 operator^(const Subset128 &a, const Subset128 &b) {
  Subset128 ret;
  for (size_t i = 0; i < 4; ++i) {
    ret.dlog[i] = _mm256_xor_si256(a.dlog[i], b.dlog[i]);
  }
  return ret;
}

/////////////////////////////////////////////////////////////////////
////////////////     Helper functions     ///////////////////////////
/////////////////////////////////////////////////////////////////////

void Subset128::print_poly() const {
  uint8_t *arr = (uint8_t *)this->dlog.data();
  for (uint32_t i = 0; i < N; ++i)
    printf("%d x^%d + ", arr[i], i);
  printf("\n");
}

void Subset128::get_full(uint8_t *arr) {
  memcpy(arr, (uint8_t *)this->dlog.data(), N);
}

uint8_t Subset128::get(size_t idx) {
  uint8_t *arr = (uint8_t *)this->dlog.data();
  uint8_t out = arr[idx];
  return out;
}

void Subset128::get_int_repr(uint8_t *out) {
  uint8_t *arr = (uint8_t *)this->dlog.data();
  for (size_t i = 0; i < N; ++i) {
    out[i] = arr[i];
  }
}

void Subset128::get_int_from_subsetsum(uint16_t *out) {
  /*
   * get dlog result and converts it x from subset sum form 3^x to j=3^x subset
   * product
   */
  uint8_t *arr = (uint8_t *)this->dlog.data();
  for (size_t i = 0; i < N; ++i) {
    out[i] = generatorPowers[arr[i]];
  }
}
} // namespace LEAP
