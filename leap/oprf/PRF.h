#include <cryptoTools/Common/BitVector.h>
#include <leap/oprf/BCH.h>
#include <leap/oprf/Subset128.h>
#include <leap/oprf/ntt-falcon.h>

#ifndef LEAP_PRF_H
#define LEAP_PRF_H
namespace LEAP {
uint64_t prf(const osuCrypto::block &input,
             const std::vector<LEAP::Subset128> &private_keys);
uint64_t compute_digest(SHAKE256 &shake256, const uint64_t input);

} // namespace LEAP
#endif // LEAP_PRF_H
