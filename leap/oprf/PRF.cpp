#include <leap/oprf/PRF.h>

namespace LEAP {
uint64_t prf(const osuCrypto::block &input,
             const std::vector<LEAP::Subset128> &private_keys) {
  LEAP::Subset128 result = private_keys[0];
  osuCrypto::BitVector bv;
  bv.assign(input);

  for (size_t j = 0; j < N; ++j) {
    if (bv[j]) {
      result = result + private_keys[j + 1];
    }
  }

  // uint8_t oout[N];
  // result.get_full(oout);
  // printf("]\n");
  // to subset-product
  uint16_t out[N] = {0};
  result.get_int_from_subsetsum(out);

  // inverse NTT

  LEAP::intt(out);
  // printf("rr=[");
  // for (size_t j = 0; j < N; ++j) {
  //     printf("%d, ", out[j]);
  // }
  // puts("]");

  uint64_t biased[2] = {0};
  for (uint64_t k = 0; k < N / 2; ++k) {
    if ((out[k] > 63) && (out[k] < 192)) {
      // 1
      biased[0] ^= ((uint64_t)1 << k);
    }
  }
  for (size_t k = N / 2; k < N; ++k) {
    if (out[k] > 63 && out[k] < 192) {
      // 1
      biased[1] ^= ((uint64_t)1 << (k - (N / 2)));
    }
  }
  // printf("%ld %ld \n", biased[0], biased[1]);
  return BCH128to64(biased);
}
uint64_t compute_digest(SHAKE256 &shake256, const uint64_t input) {
  uint64_t output;
  shake256.Reset(8);
  shake256.Update((uint8_t *)&input, 8);
  shake256.Final((uint8_t *)&output);
  return output;
}

} // namespace LEAP
