#pragma once

#include <coproto/Socket/AsioSocket.h>
#include <cryptoTools/Common/Matrix.h>
#include <leap/oprf/BCH.h>
#include <leap/oprf/PRF.h>
#include <leap/oprf/Subset128.h>
#include <leap/oprf/ntt-falcon.h>
#include <leap/psi/cuckoofilter/cuckoofilter.h>
#include <leap/psi/params.h>

#ifdef KYBER_OT
#include <libOTe/Base/MasnyRindalKyber.h>
#else
#include <libOTe/Base/SimplestOT.h>
#endif
#ifdef IKNP
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h>
#else
#include <libOTe/TwoChooseOne/Silent/SilentOtExtSender.h>
#endif

using namespace osuCrypto;

namespace LEAP {

class SpringNRPSIServer {
public:
  SpringNRPSIServer(cp::Socket &sock, size_t num_threads = 1);
  ~SpringNRPSIServer();

  void Setup(std::vector<block> &elements);
  void Base();
  void Online();

private:
  PRNG prng;
  cp::Socket socket;
  size_t num_client_elements_;
  std::vector<Subset128> private_keys;
  std::vector<std::array<block, 2>> ots_;
  const size_t numBaseOTs = 128;
  std::vector<Subset128> r_0;
  std::vector<Subset128> r_1;
  std::vector<uint16_t> ole_r0;
  std::vector<uint16_t> ole_r1;
  std::vector<uint16_t> round_r0;
  std::vector<uint16_t> round_r1;
  size_t num_threads_;
  SHAKE256 shake256;
};
} // namespace LEAP
