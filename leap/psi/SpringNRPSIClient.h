#pragma once

#include <coproto/Socket/AsioSocket.h>
#include <cryptoTools/Common/Matrix.h>
#include <leap/oprf/BCH.h>
#include <leap/oprf/PRF.h>
#include <leap/oprf/Subset128.h>
#include <leap/oprf/ntt-falcon.h>
#include <leap/psi/params.h>
#ifdef KYBER_OT
#include <libOTe/Base/MasnyRindalKyber.h>
#else
#include <libOTe/Base/SimplestOT.h>
#endif
#ifdef IKNP
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h>
#else
#include <libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h>
#endif


#include "cuckoofilter/cuckoofilter.h"

using namespace osuCrypto;

namespace LEAP {
class SpringNRPSIClient {
public:
  SpringNRPSIClient(cp::AsioSocket &sock);

  virtual ~SpringNRPSIClient() { delete cf_; }

  void Setup();
  void Base(size_t num_elements);
  std::vector<size_t> Online(std::vector<block> &elements);

private:
  std::vector<Subset128> ot_ss;
  std::vector<block> ots_;
  std::vector<uint16_t> ole_int;
  std::vector<uint64_t> round_r;
  BitVector ot_choices_;
  SHAKE256 shake256;
  PRNG prng;
  cp::AsioSocket socket;
  const size_t numBaseOTs = 128;
  typedef cuckoofilter::CuckooFilter<
      uint64_t *, 32, cuckoofilter::SingleTable,
      cuckoofilter::TwoIndependentMultiplyShift128>
      CuckooFilter;
  CuckooFilter *cf_;
};
} // namespace LEAP
