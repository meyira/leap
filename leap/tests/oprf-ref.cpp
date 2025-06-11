#include <coproto/Socket/AsioSocket.h>
#include <cryptoTools/Common/Matrix.h>
#include <cstring>
#include <leap/oprf/BCH.h>
#include <leap/oprf/PRF.h>
#include <leap/oprf/Subset128.h>
#include <leap/oprf/ntt-falcon.h>
#include <libOTe/Base/MasnyRindalKyber.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h>
#include <unistd.h>

#define LEAP_MICRO_BENCHMARKS

using namespace osuCrypto;
using namespace LEAP;

#define HASHLEN N

int main() {
  const uint64_t N = 128;
  const size_t numBaseOTs = 128;
  std::string ip = "127.0.0.1:12112";

  auto recverThread = std::thread([&]() {
    // setup network, PRNG, fixed input for testing
    cp::Socket client_socket = coproto::asioConnect(ip, true);
    PRNG prng(sysRandomSeed());
    SHAKE256 shake256;
    Timer time;
    osuCrypto::block input =
        osuCrypto::toBlock((const uint8_t *)"ffffffff88888888");

    ////////////////////////////////////////////////////
    ////////////////// Base OT    //////////////////////
    ////////////////////////////////////////////////////
    auto base_start = client_socket.bytesSent();
    auto cc_bot = time.setTimePoint("bot");
    osuCrypto::BitVector ot_choices_;
    std::vector<osuCrypto::block> ots_;

    std::vector<std::array<block, 2>> baseOTs;
    baseOTs.resize(numBaseOTs);
    osuCrypto::span<std::array<block, 2>> baseOTsSpan(baseOTs.data(),
                                                      baseOTs.size());

    // base OT
    MasnyRindalKyber ot;
    coproto::sync_wait(ot.send(baseOTsSpan, prng, client_socket));

    // OT extension
    IknpOtExtReceiver OTeRecv;
    OTeRecv.setBaseOts(baseOTsSpan);
    ot_choices_.resize(N * 19);
    ot_choices_.randomize(prng);
    ots_.resize(ot_choices_.size());
    osuCrypto::span<osuCrypto::block> otSpan(ots_.data(), ots_.size());
    coproto::sync_wait(
        OTeRecv.receive(ot_choices_, otSpan, prng, client_socket));

    // generate blinding polynomials for subset-sum step
    std::vector<Subset128> ot_res;
    for (size_t j = 0; j < N; ++j) {
      Subset128 res(ots_[j], &shake256);
      ot_res.push_back(res);
    }

    std::array<uint16_t, N * LOG_Q> ole_int{0};
    for (size_t j = 0; j < N * LOG_Q; ++j) {
      // generate OLE elements
      shake256.Reset(2);
      shake256.Update((uint8_t *)&ots_[N + j], 128 / 8);
      shake256.Final((uint8_t *)&ole_int[j]);
      ole_int[j] = ole_int[j] & 0x1ff;
      while (ole_int[j] >= SPRING_Q) {
        // rejection sampling to get OLE input in [0, SPRING_Q]
        shake256.Final((uint8_t *)&ole_int[j]);
        ole_int[j] = ole_int[j] & 0x1ff;
      }
    }

    auto base_end = client_socket.bytesSent();

    ////////////////////////////////////////////////////
    ////////////////// Subset-Sum    ///////////////////
    ////////////////////////////////////////////////////

    auto cc_subsum_prep = time.setTimePoint("subsum-prep");
    Subset128 aggregated;
    std::vector<uint8_t> received(128 * N);
    auto recvSpan =
        osuCrypto::span<uint8_t>((uint8_t *)received.data(), 128 * N);
    BitVector server_bv;
    server_bv.copy(ot_choices_, 0, N);
    osuCrypto::BitVector bv;
    bv.assign(input);
    server_bv = server_bv ^ bv;
    auto serverBvSpan =
        osuCrypto::span<uint8_t>(server_bv.data(), server_bv.sizeBytes());

#ifdef LEAP_MICRO_BENCHMARKS
    auto cc_subsum_wait = time.setTimePoint("subsum-wait");
#endif
    coproto::sync_wait(client_socket.send(serverBvSpan));
    coproto::sync_wait(client_socket.recv(recvSpan));
#ifdef LEAP_MICRO_BENCHMARKS
    auto cc_subsum = time.setTimePoint("subsum");
#endif
    uint8_t *r_ptr = received.data();
    for (size_t j = 0; j < N; ++j) {
      Subset128 r(r_ptr);
      if (bv[j]) {
        aggregated = aggregated + (ot_res[j] ^ r);
      } else {
        aggregated = aggregated + ot_res[j];
      }
      r_ptr += N;
    }

    auto cc_end = client_socket.bytesSent();

    ////////////////////////////////////////////////////
    //////////////// Client OLE  ///////////////////////
    ////////////////////////////////////////////////////
    auto cc_ole_prep = time.setTimePoint("ole-prep");
    // lift to subset product for OLE
    std::array<uint16_t, N> ole_in;
    aggregated.get_int_from_subsetsum(ole_in.data());

    // allocate 16*8*N bits for the correction vector
    std::array<uint16_t, N> y;
    size_t idx = 0;
    BitVector ole_corr(N * LOG_Q);
    for (uint16_t j : ole_in) {
      for (size_t k = 0; k < LOG_Q; ++k) {
        ole_corr[idx] = (j & (1 << k)) >> k;
        ++idx;
      }
    }
    // Inititialize BitVector with length bits pointed to by data.
    BitVector ole_choices;
    // get bitvector at correct offset
    ole_choices.copy(ot_choices_, N, N * LOG_Q);
    // XOR with choice bits
    ole_choices = ole_corr ^ ole_choices;
    auto oleSpan =
        osuCrypto::span<uint8_t>(ole_choices.data(), ole_choices.sizeBytes());
  // use uint32_t to avoid overflows
    std::vector<uint32_t> ole_enc(1152);

#ifdef LEAP_MICRO_BENCHMARKS
    auto cc_ole_wait = time.setTimePoint("ole-wait");
#endif
    coproto::sync_wait(client_socket.send(oleSpan));
    coproto::sync_wait(client_socket.recv(ole_enc));
#ifdef LEAP_MICRO_BENCHMARKS
    auto cc_ole = time.setTimePoint("ole");
#endif

    idx = 0;
    for (uint16_t &j : y) {
      uint32_t result = 0;
      for (size_t k = 0; k < LOG_Q; ++k) {
        if (ole_corr[idx]) {
          result = result + (((uint32_t)ole_int[idx]<<k) ^
                             (uint32_t)ole_enc[idx]);
        } else {
          result = result + ((uint32_t)ole_int[idx]<<k);
        }
        ++idx;
      }
      j = result % SPRING_Q;
    }

    auto ole_end = client_socket.bytesSent();
    auto cc_ntt = time.setTimePoint("ntt");
    intt(y.data());

    ////////////////////////////////////////////////////
    ////////////////// Client Rounding  ////////////////
    ////////////////////////////////////////////////////

    auto cc_rounding_prep = time.setTimePoint("round-prep");

    std::vector<block> recvMsgs(N);
    BitVector rounding_corr(1152);
    idx = 0;
    for (uint16_t j : y) {
      for (int64_t k = 8; k >= 0; --k) {
        rounding_corr[idx] = (j & (1 << k)) >> k;
        ++idx;
      }
    }

    // Inititialize BitVector with length bits pointed to by data.
    BitVector rounding_choices;
    // get bitvector at correct offset
    rounding_choices.copy(ot_choices_, N * 10, 1152);
    // XOR with choice bits
    rounding_choices = rounding_corr ^ rounding_choices;
    auto roundSpan = osuCrypto::span<uint8_t>(rounding_choices.data(),
                                              rounding_choices.sizeBytes());
    std::array<uint64_t, 2> biased{0};
    std::array<uint64_t, 2 * SPRING_Q> enc_bits{0};
    uint64_t mask;
#ifdef LEAP_MICRO_BENCHMARKS
    auto cc_rounding_wait = time.setTimePoint("round-wait");
#endif
    coproto::sync_wait(client_socket.send(roundSpan));
    coproto::sync_wait(client_socket.recv(enc_bits));
    coproto::sync_wait(client_socket.recv(mask));
#ifdef LEAP_MICRO_BENCHMARKS
    auto cc_round = time.setTimePoint("round");
#endif

    for (size_t j = 0; j < 64; ++j) {
      // note: the compiler may claim this variable is initialized and unused,
      // this is not the case as the loop below will always be executed
      uint8_t kee = 0;
      for (size_t l = 0; l < LOG_Q; ++l) {
        uint8_t *extract = (uint8_t *)&ots_[N * 10 + j * LOG_Q + l];
        kee ^= (extract[0] & 1);
      }
      biased[0] ^= (((enc_bits[y[j] * 2] >> j) & 1) ^ (uint64_t)kee) << j;
    }
    for (size_t j = 64; j < N; ++j) {
      // note: the compiler may claim this variable is initialized and unused,
      // this is not the case as the loop below will always be executed
      uint8_t kee = 0;
      for (size_t l = 0; l < LOG_Q; ++l) {
        uint8_t *extract = (uint8_t *)&ots_[N * 10 + j * LOG_Q + l];
        kee ^= (extract[0] & 1);
      }
      biased[1] |= (((enc_bits[y[j] * 2 + 1] >> (j - 64)) & 1) ^ (uint64_t)kee)
                   << (j - 64);
    }
    
    auto rounding_end = client_socket.bytesSent();
    auto cc_bch = time.setTimePoint("bch");
    uint64_t prf_out = mask ^ BCH128to64(biased.data());
    auto bch_end = client_socket.bytesSent();
    auto digest = LEAP::compute_digest(shake256, prf_out);
    auto postEnd = time.setTimePoint("dig-end");
    printf("[OPRF] Result: %ld\n", prf_out);
    printf("[OPRF] Digest: %ld\n", digest);

    /*
     * End of functional code, only benchmarks
     */

    auto overall =
        std::chrono::duration_cast<std::chrono::milliseconds>(postEnd - cc_bot)
            .count();
    auto b_t = std::chrono::duration_cast<std::chrono::milliseconds>(
                   cc_subsum_prep - cc_bot)
                   .count();
    auto sub_all_t = std::chrono::duration_cast<std::chrono::microseconds>(
                         cc_ole_prep - cc_subsum_prep)
                         .count();
    auto ole_all_t = std::chrono::duration_cast<std::chrono::microseconds>(
                         cc_rounding_prep - cc_ole_prep)
                         .count();
    auto ntt_t = std::chrono::duration_cast<std::chrono::microseconds>(
                     cc_rounding_prep - cc_ntt)
                     .count();
    auto rounding_t = std::chrono::duration_cast<std::chrono::microseconds>(
                          cc_bch - cc_rounding_prep)
                          .count();
    auto bch_t =
        std::chrono::duration_cast<std::chrono::microseconds>(postEnd - cc_bch)
            .count();
#ifdef LEAP_MICRO_BENCHMARKS
    auto subsum_prep_t = std::chrono::duration_cast<std::chrono::microseconds>(
                             cc_subsum_wait - cc_subsum_prep)
                             .count();
    auto subsum_wait_t = std::chrono::duration_cast<std::chrono::microseconds>(
                             cc_subsum - cc_subsum_wait)
                             .count();
    auto subsum_t = std::chrono::duration_cast<std::chrono::microseconds>(
                        cc_ole_prep - cc_subsum)
                        .count();
    auto ole_prep_t = std::chrono::duration_cast<std::chrono::microseconds>(
                          cc_ole_wait - cc_ole_prep)
                          .count();
    auto ole_wait_t = std::chrono::duration_cast<std::chrono::microseconds>(
                          cc_ole - cc_ole_wait)
                          .count();
    auto ole_t =
        std::chrono::duration_cast<std::chrono::microseconds>(cc_ntt - cc_ole)
            .count();
    auto rounding_all_t =
        std::chrono::duration_cast<std::chrono::microseconds>(cc_bch - cc_round)
            .count();
    auto rounding_prep_t =
        std::chrono::duration_cast<std::chrono::microseconds>(cc_rounding_wait -
                                                              cc_rounding_prep)
            .count();
    auto rounding_wait_t =
        std::chrono::duration_cast<std::chrono::microseconds>(cc_round -
                                                              cc_rounding_wait)
            .count();
#endif

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "[Client] overall " << overall << " ms, baseOT " << b_t
              << " ms, Subset-Sum " << sub_all_t << " us, OLE " << ole_all_t
              << " us, NTT " << ntt_t << " us, rounding " << rounding_t
              << " us, BCH + postprocessing " << bch_t << " us" << std::endl;
#ifdef LEAP_MICRO_BENCHMARKS
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "[Client] Microbenchmarks: \n\t Subsum:  " << subsum_prep_t
              << " us preparation, " << subsum_wait_t
              << " us waiting for server answer, " << subsum_t
              << " us computation\n\t OLE: " << ole_prep_t
              << " us preparation, " << ole_wait_t
              << " us waiting for server answer, " << ole_t
              << " us computation\n\t Rounding: " << rounding_prep_t
              << " us preparation, " << rounding_wait_t
              << " us waiting for server answer, " << rounding_all_t
              << " us computation" << std::endl;
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "[Client] overall " << (bch_end - base_start) / 1024.0
              << " kiB, BaseOT " << (base_end - base_start) / 1024.0
              << " kiB,  online " << (bch_end - base_end)
              << " bytes,  Subset-Sum " << cc_end - base_end
              << " bytes,  OLE comms " << ole_end - cc_end
              << " bytes, Rounding Comms " << rounding_end - ole_end
              << " bytes,  BCH Comms " << bch_end - rounding_end << " bytes"
              << std::endl;
  });

  Timer time;
  PRNG prng(sysRandomSeed());
  SHAKE256 shake256;
  std::vector<Subset128> private_keys;

  ////////////////////////////////////////////////////
  ////////////////// Setup    ////////////////////////
  ////////////////////////////////////////////////////
  auto time_start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < HASHLEN + 1; ++i) {
    Subset128 rand;
    rand.rand_poly(&prng);
    private_keys.push_back(rand);
  }
  auto time_end = std::chrono::high_resolution_clock::now();
  std::chrono::microseconds time_keygen_micro =
      std::chrono::duration_cast<std::chrono::microseconds>(time_end -
                                                            time_start);

  std::cout << "SPRING KeyGen Time: " << time_keygen_micro.count()
            << " microseconds" << std::endl;

  osuCrypto::block sanity =
      osuCrypto::toBlock((const uint8_t *)"ffffffff88888888");
  auto time0 = std::chrono::high_resolution_clock::now();
  uint64_t prf_out = LEAP::prf(sanity, private_keys);
  auto time4 = std::chrono::high_resolution_clock::now();
  printf("[PRF] %ld\n", prf_out);

  cp::Socket server_socket = coproto::asioConnect(ip, false);

  ////////////////////////////////////////////////////
  ////////////////// Base OT    //////////////////////
  ////////////////////////////////////////////////////
  auto base_start = server_socket.bytesSent();
  auto server_bot = time.setTimePoint("bot");
  std::vector<block> baseOTs;
  BitVector baseChoices(numBaseOTs);
  baseChoices.randomize(prng);
  baseOTs.resize(numBaseOTs);
  std::vector<std::array<osuCrypto::block, 2>> ots_;
  osuCrypto::span<block> baseOTsSpan(baseOTs.data(), baseOTs.size());

  MasnyRindalKyber ot;
  coproto::sync_wait(ot.receive(baseChoices, baseOTsSpan, prng, server_socket));

  osuCrypto::IknpOtExtSender otExtSender;
  otExtSender.setBaseOts(baseOTsSpan, baseChoices);

  // one for subset-sum, one for each gilboa coefficient bit, one for each
  // rounding
  ots_.resize(N * 19);
  osuCrypto::span<std::array<osuCrypto::block, 2>> otSpan(ots_.data(),
                                                          ots_.size());
  coproto::sync_wait(otExtSender.send(otSpan, prng, server_socket));

  std::vector<Subset128> r_0;
  std::vector<Subset128> r_1;
  for (size_t j = 0; j < N; ++j) {
    Subset128 r0(ots_[j][0], &shake256);
    Subset128 r1(ots_[j][1], &shake256);
    r_0.push_back(r0);
    r_1.push_back(r1);
  }

  std::array<uint16_t, N * LOG_Q> ole_r0;
  std::array<uint16_t, N * LOG_Q> ole_r1;
  for (size_t j = 0; j < N * LOG_Q; ++j) {
    // rejection sampling routine
    shake256.Reset(2);
    shake256.Update((uint8_t *)&ots_[N + j][0], 128 / 8);
    shake256.Final((uint8_t *)&ole_r0[j]);
    ole_r0[j] = ole_r0[j] & 0x1ff;
    while (ole_r0[j] >= SPRING_Q) {
      shake256.Final((uint8_t *)&ole_r0[j]);
      ole_r0[j] = ole_r0[j] & 0x1ff;
    }

    shake256.Reset(2);
    shake256.Update((uint8_t *)&ots_[N + j][1], 128 / 8);
    shake256.Final((uint8_t *)&ole_r1[j]);
    ole_r1[j] = ole_r1[j] & 0x1ff;
    while (ole_r1[j] >= SPRING_Q) {
      shake256.Final((uint8_t *)&ole_r1[j]);
      ole_r1[j] = ole_r1[j] & 0x1ff;
    }
  }

  auto base_end = server_socket.bytesSent();

  ////////////////////////////////////////////////////
  ////////////////// Subset Sum //////////////////////
  ////////////////////////////////////////////////////
  // vectors of server_choices for PSI
  auto server_subsum_prep = time.setTimePoint("subsum-prep");
  osuCrypto::BitVector subsum_choices(N);
  auto serverSpan = osuCrypto::span<uint8_t>(subsum_choices.data(),
                                             subsum_choices.sizeBytes());

#ifdef LEAP_MICRO_BENCHMARKS
  auto server_subsum_wait = time.setTimePoint("subsum-wait");
#endif
  coproto::sync_wait(server_socket.recv(serverSpan));
#ifdef LEAP_MICRO_BENCHMARKS
  auto server_subsum = time.setTimePoint("subset-sum");
#endif

  Subset128 s_prime = private_keys[0];
  std::vector<uint8_t> send_poly(128 * N);
  for (size_t j = 0; j < N; ++j) {
    Subset128 randomness;
    if (subsum_choices[j]) {
      s_prime = s_prime - r_1[j];
      randomness = r_0[j] ^ (r_1[j] + private_keys[j + 1]);
    } else {
      s_prime = s_prime - r_0[j];
      randomness = r_1[j] ^ (r_0[j] + private_keys[j + 1]);
    }
    memcpy(send_poly.data() + j * N, (uint8_t *)randomness.dlog.data(), N);
  }
  auto polySpan =
      osuCrypto::span<uint8_t>((uint8_t *)send_poly.data(), 128 * N);
#ifdef LEAP_MICRO_BENCHMARKS
  auto server_subsum_wait_two = time.setTimePoint("subsum-wait-2");
#endif
  coproto::sync_wait(server_socket.send(polySpan));
  auto server_end = server_socket.bytesSent();

  //////////////////////////////////////////////////
  //////////////// OLE    //////////////////////////
  //////////////////////////////////////////////////
  auto server_ole_prep = time.setTimePoint("ole-prep");

  std::array<uint16_t, N> ole_in;
  s_prime.get_int_from_subsetsum(ole_in.data());
  std::array<uint16_t, N> blinder;
  // use uint32_t to avoid overflows
  std::array<uint32_t, 1152> ole_enc;
  BitVector ole_corr(1152);

  auto oleSpan =
      osuCrypto::span<uint8_t>(ole_corr.data(), ole_corr.sizeBytes());
#ifdef LEAP_MICRO_BENCHMARKS
  auto server_ole_wait = time.setTimePoint("ole-wait");
#endif
  coproto::sync_wait(server_socket.recv(oleSpan));
#ifdef LEAP_MICRO_BENCHMARKS
  auto server_ole = time.setTimePoint("ole");
#endif
  for (size_t j = 0; j < N; ++j) {
    uint64_t blinding_factor = 0;
    for (size_t k = 0; k < LOG_Q; k++) {
      // check if correction bit was set
      if (ole_corr[j * LOG_Q + k]) {
        // update blinding factor with r1 << k
        blinding_factor += (ole_r1[j * LOG_Q + k]<<k);
        // encrypt OLE result with ole_r1 and XOR it with r0 
         ole_enc[j * LOG_Q + k] =
            ((uint32_t)ole_r0[j * LOG_Q + k]<<k) ^
            (((uint32_t)ole_in[j] + (uint32_t)ole_r1[j * LOG_Q + k])<<k);
      } else {
        // update blinding factor with r1 << k
        blinding_factor += (ole_r0[j * LOG_Q + k]<<k);
        ole_enc[j * LOG_Q + k] =
            ((uint32_t)ole_r1[j * LOG_Q + k]<<k) ^
            (((uint32_t)ole_in[j] + (uint32_t)ole_r0[j * LOG_Q + k])<<k);
      }
    }
    blinder[j] = blinding_factor % SPRING_Q;
  }
#ifdef LEAP_MICRO_BENCHMARKS
  auto server_ole_wait_two = time.setTimePoint("ole-wait-2");
#endif
  coproto::sync_wait(server_socket.send(ole_enc));
  auto ole_end = server_socket.bytesSent();

  auto server_ntt = time.setTimePoint("ntt");

  intt(blinder.data());

  ////////////////////////////////////////////////////
  ////////////////// Rounding    /////////////////////
  ////////////////////////////////////////////////////
  auto server_rounding_prep = time.setTimePoint("round-prep");
  BitVector rounding_corr(1152);
  auto roundSpan =
      osuCrypto::span<uint8_t>(rounding_corr.data(), rounding_corr.sizeBytes());
#ifdef LEAP_MICRO_BENCHMARKS
  auto server_rounding_wait = time.setTimePoint("round-wait");
#endif
  coproto::sync_wait(server_socket.recv(roundSpan));
#ifdef LEAP_MICRO_BENCHMARKS
  auto server_rounding = time.setTimePoint("round");
#endif

  // 128 poly coefficients, for each we have SPRING_Q options: (128*SPRING_Q)/64=SPRING_Q*2
  std::array<uint64_t, SPRING_Q * 2> enc_bits{0};

  std::array<uint64_t, 2> mask{0};
  for (uint64_t &j : mask) {
    j = prng.get<uint64_t>();
  }

  /*
   * XOR rounding result with OT keys
   * encrypt meserverages s.t. enc_i=m_i^(e(1^{i_j})
   */
  for (int32_t k = 0; k < SPRING_Q; k++) {
    for (size_t j = 0; j < 64; ++j) {
      uint16_t r0 = 0;
      uint16_t r1 = 0;
      for (size_t l = 0; l < LOG_Q; ++l) {
        uint8_t *extract = (uint8_t *)&ots_[N * 10 + j * LOG_Q + l][0];
        r0 |= (extract[0] & 1) << l;
        extract = (uint8_t *)&ots_[N * 10 + j * LOG_Q + l][1];
        r1 |= (extract[0] & 1) << l;
      }
      uint8_t bit = 0;
      // compute assumed rounding value
      int32_t to_round = (int32_t)(k - (int32_t)blinder[j]);
      while (to_round < 0) {
        to_round += SPRING_Q;
      }
      while (to_round >= SPRING_Q) {
        to_round -= SPRING_Q;
      }
      if ((to_round > 63) && (to_round < 193)) {
        bit ^= 1;
      }
      // encrypt result bit with OT taking corrections into account
      for (size_t l = 0; l < LOG_Q; ++l) {
        if (rounding_corr[j * LOG_Q + l]) {
          // correction bit set, switch how encryption works
          if ((k >> (LOG_Q - l - 1)) & 1) {
            bit ^= ((r0 >> l) & 1);
          } else {
            bit ^= ((r1 >> l) & 1);
          }
        } else {
          // correct bit was already requested
          if ((k >> (LOG_Q - l - 1)) & 1) {
            bit ^= ((r1 >> l) & 1);
          } else {
            bit ^= ((r0 >> l) & 1);
          }
        }
      }
      enc_bits[2 * k] |= ((uint64_t)bit << j);
    }
    enc_bits[2 * k] ^= mask[0];
    for (size_t j = 64; j < N; ++j) {
      uint16_t r0 = 0;
      uint16_t r1 = 0;
      for (size_t l = 0; l < LOG_Q; ++l) {
        uint8_t *extract = (uint8_t *)&ots_[N * 10 + j * LOG_Q + l][0];
        r0 |= (extract[0] & 1) << l;
        extract = (uint8_t *)&ots_[N * 10 + j * LOG_Q + l][1];
        r1 |= (extract[0] & 1) << l;
      }
      uint8_t bit = 0;
      // compute assumed rounding value
      int32_t to_round = (int32_t)(k - (int32_t)blinder[j]);
      while (to_round < 0) {
        to_round += SPRING_Q;
      }
      while (to_round >= SPRING_Q) {
        to_round -= SPRING_Q;
      }
      if ((to_round > 63) && (to_round < 193))
        // round up if centered
        bit ^= 1;
      for (size_t l = 0; l < LOG_Q; ++l) {
        if (rounding_corr[j * LOG_Q + l]) {
          // switch
          if ((k >> (LOG_Q - l - 1)) & 1) {
            bit ^= ((r0 >> l) & 1);
          } else {
            bit ^= ((r1 >> l) & 1);
          }
        } else {
          if ((k >> (LOG_Q - l - 1)) & 1) {
            bit ^= ((r1 >> l) & 1);
          } else {
            bit ^= ((r0 >> l) & 1);
          }
        }
      }
      enc_bits[2 * k + 1] |= ((uint64_t)bit << (j - 64));
    }
    enc_bits[2 * k + 1] ^= mask[1];
  }

#ifdef LEAP_MICRO_BENCHMARKS
  auto server_rounding_wait_two = time.setTimePoint("round-end");
#endif
  coproto::sync_wait(server_socket.send(enc_bits));
  auto rounding_end = server_socket.bytesSent();

  auto server_bch = time.setTimePoint("bch");
  uint64_t bch_mask = BCH128to64(mask.data());
  coproto::sync_wait(server_socket.send(bch_mask));
  auto server_fin = time.setTimePoint("end");
  auto bch_end = server_socket.bytesSent();

  /*
   * End of functional code, only benchmarks
   */

  auto overall = std::chrono::duration_cast<std::chrono::milliseconds>(
                     server_fin - server_bot)
                     .count();
  auto prf_t =
      std::chrono::duration_cast<std::chrono::milliseconds>(time4 - time0)
          .count();
  auto b_t = std::chrono::duration_cast<std::chrono::milliseconds>(
                 server_subsum_prep - server_bot)
                 .count();
  auto sub_all_t = std::chrono::duration_cast<std::chrono::microseconds>(
                       server_ole_prep - server_subsum_prep)
                       .count();
  auto ole_all_t = std::chrono::duration_cast<std::chrono::microseconds>(
                       server_rounding_prep - server_ole_prep)
                       .count();
  auto rounding_all_t = std::chrono::duration_cast<std::chrono::microseconds>(
                            server_bch - server_rounding_prep)
                            .count();
  auto bch_t = std::chrono::duration_cast<std::chrono::microseconds>(
                   server_fin - server_bch)
                   .count();
  auto ntt_t = std::chrono::duration_cast<std::chrono::microseconds>(
                   server_rounding_prep - server_ntt)
                   .count();

#ifdef LEAP_MICRO_BENCHMARKS
  auto subsum_prep_t = std::chrono::duration_cast<std::chrono::microseconds>(
                           server_subsum_wait - server_subsum_prep)
                           .count();
  auto subsum_wait_t = std::chrono::duration_cast<std::chrono::microseconds>(
                           server_subsum - server_subsum_wait +
                           server_ole_prep - server_subsum_wait_two)
                           .count();
  auto subsum_t = std::chrono::duration_cast<std::chrono::microseconds>(
                      server_subsum_wait_two - server_subsum)
                      .count();

  auto ole_prep_t = std::chrono::duration_cast<std::chrono::microseconds>(
                        server_ole_wait - server_ole_prep)
                        .count();
  auto ole_wait_t = std::chrono::duration_cast<std::chrono::microseconds>(
                        server_ole - server_ole_wait + server_rounding_prep -
                        server_ole_wait_two)
                        .count();
  auto ole_t = std::chrono::duration_cast<std::chrono::microseconds>(
                   server_ole_wait_two - server_ole)
                   .count();

  auto rounding_prep_t = std::chrono::duration_cast<std::chrono::microseconds>(
                             server_rounding_wait - server_rounding_prep)
                             .count();
  auto rounding_t = std::chrono::duration_cast<std::chrono::microseconds>(
                        server_rounding_wait_two - server_rounding)
                        .count();
  auto rounding_wait_t = std::chrono::duration_cast<std::chrono::microseconds>(
                             server_rounding - server_rounding_wait +
                             server_bch - server_rounding_wait_two)
                             .count();
#endif

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::cout << "[Server] overall " << overall << " ms, PRF " << prf_t
            << " ms, baseOT " << b_t << " ms, Subset - Sum " << sub_all_t
            << " us, OLE " << ole_all_t << " us, NTT " << ntt_t
            << " us, rounding " << rounding_all_t << " us, BCH " << bch_t
            << " us" << std::endl;
#ifdef LEAP_MICRO_BENCHMARKS
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::cout << "[Server] Microbenchmarks: \n\t Subsum:  " << subsum_prep_t
            << "us preparation, " << subsum_wait_t
            << " us waiting for client answer, " << subsum_t
            << " us computation\n\t OLE: " << ole_prep_t << "us preparation, "
            << ole_wait_t << " us waiting for client answer, " << ole_t
            << " us computation\n\t Rounding: " << rounding_prep_t
            << "us preparation, " << rounding_wait_t
            << " us waiting for client answer, " << rounding_t
            << " us computation" << std::endl;
#endif
  std::cout << "[Server] overall " << (bch_end - base_start) / 1024.0
            << " kiB, BaseOT " << (base_end - base_start) / 1024.0
            << " kiB,  online " << (bch_end - base_end)
            << " bytes,  Subset-Sum " << server_end - base_end
            << " bytes,  OLE comms " << ole_end - server_end
            << " bytes, Rounding Comms " << rounding_end - ole_end
            << " bytes, BCH Comms " << bch_end - rounding_end << " bytes"
            << std::endl;
  recverThread.join();
  return 0;
}
