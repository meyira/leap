#include "iter.h"
#include <cryptoTools/Common/Matrix.h>
#include <leap/oprf/Subset128.h>
#include <libOTe/Base/SimplestOT.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h>

#include <cstring>

using namespace osuCrypto;
using namespace LEAP;

#define HASHLEN N

int main() {
  auto sockets = cp::LocalAsyncSocket::makePair();

  const uint64_t N = 128;
  const size_t numBaseOTs = 128;
  auto recverThread = std::thread([&]() {
    PRNG prng(sysRandomSeed());
    SHAKE256 shake256;
    Timer time;

    ////////////////////////////////////////////////////
    ////////////////// Base OT    //////////////////////
    ////////////////////////////////////////////////////
    auto base_start = sockets[1].bytesSent();
    auto cc_bot = time.setTimePoint("bot");
    osuCrypto::BitVector ot_choices_;
    std::vector<osuCrypto::block> ots_;

    std::vector<std::array<block, 2>> baseOTs;
    baseOTs.resize(numBaseOTs);
    osuCrypto::span<std::array<block, 2>> baseOTsSpan(baseOTs.data(),
                                                      baseOTs.size());

    AsmSimplestOT ot;
    coproto::sync_wait(ot.send(baseOTsSpan, prng, sockets[1]));

    IknpOtExtReceiver OTeRecv;
    OTeRecv.setBaseOts(baseOTsSpan);

    ot_choices_.resize(numPRF * (N * 10 + 1152));
    ot_choices_.randomize(prng);
    ots_.resize(numPRF * (N * 10 + 1152));
    osuCrypto::span<osuCrypto::block> otSpan(ots_.data(), ots_.size());
    coproto::sync_wait(OTeRecv.receive(ot_choices_, otSpan, prng, sockets[1]));
    std::vector<Subset128> ot_res;
    for (size_t j = 0; j < N; ++j) {
      Subset128 res(otSpan[j], &shake256);
      ot_res.push_back(res);
    }

    auto cc_bot_fin = time.setTimePoint("bot-fin");
    auto base_end = sockets[1].bytesSent();
    auto b_t = std::chrono::duration_cast<std::chrono::milliseconds>(
                   cc_bot_fin - cc_bot)
                   .count();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "[Client] SimplestOT+IKNP " << b_t << " ms " << std::endl;
    std::cout << "[Client] SimplestOT+IKNP " << (base_end - base_start) / 1000.0
              << " kB" << std::endl;
  });

  Timer time;
  PRNG prng(sysRandomSeed());
  ////////////////////////////////////////////////////
  ////////////////// Base OT    //////////////////////
  ////////////////////////////////////////////////////
  auto base_start = sockets[0].bytesSent();
  auto ss_bot = time.setTimePoint("bot");
  std::vector<block> baseOTs;
  BitVector baseChoices(numBaseOTs);
  baseChoices.randomize(prng);
  baseOTs.resize(numBaseOTs);
  std::vector<std::array<osuCrypto::block, 2>> ots_;
  osuCrypto::span<block> baseOTsSpan(baseOTs.data(), baseOTs.size());

  AsmSimplestOT ot;
  coproto::sync_wait(ot.receive(baseChoices, baseOTsSpan, prng, sockets[0]));

  osuCrypto::IknpOtExtSender otExtSender;
  otExtSender.setBaseOts(baseOTsSpan, baseChoices);

  // one for subset-sum, one for each gilboa coefficient bit, one for each
  // rounding
  ots_.resize(numPRF * (N * 10 + 1152));
  osuCrypto::span<std::array<osuCrypto::block, 2>> otSpan(ots_.data(),
                                                          ots_.size());
  coproto::sync_wait(otExtSender.send(otSpan, prng, sockets[0]));

  auto base_end = sockets[0].bytesSent();
  auto ss_fin = time.setTimePoint("end");
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  auto overall =
      std::chrono::duration_cast<std::chrono::milliseconds>(ss_fin - ss_bot)
          .count();

  std::cout << "[Server] SimplestOT+IKNP " << overall << " ms " << std::endl;
  std::cout << "[Server] SimplestOT+IKNP " << (base_end - base_start) / 1000.0
            << " kB " << std::endl;
  recverThread.join();
  return 0;
}
