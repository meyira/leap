#include <leap/psi/SpringNRPSIServer.h>

namespace LEAP {

SpringNRPSIServer::SpringNRPSIServer(cp::Socket &sock,
                                     size_t num_threads /*=1*/)
    : prng(sysRandomSeed()), num_client_elements_(0) {
  num_threads_ = num_threads;
  this->socket = sock;
}

SpringNRPSIServer::~SpringNRPSIServer() = default;

void SpringNRPSIServer::Setup(std::vector<block> &elements) {
  std::chrono::high_resolution_clock::time_point time_start, time_end;
  time_start = std::chrono::high_resolution_clock::now();
  auto begin = socket.bytesSent();
  time_end = std::chrono::high_resolution_clock::now();
  std::chrono::microseconds time_keygen =
      std::chrono::duration_cast<std::chrono::microseconds>(time_end -
                                                            time_start);

  std::cout << "KeyGen Time: " << time_keygen.count() << " us" << std::endl;

  typedef cuckoofilter::CuckooFilter<
      uint64_t *, 32, cuckoofilter::SingleTable,
      cuckoofilter::TwoIndependentMultiplyShift128>
      CuckooFilter;

  auto time0 = std::chrono::high_resolution_clock::now();

  ////////////////////////////////////////////////////////////////////
  ///// ------- generate ALPHA private keys for NR PRF ------- ///////
  ////////////////////////////////////////////////////////////////////
  time_start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < N + 1; ++i) {
    Subset128 rand;
    rand.rand_poly(&prng);
    private_keys.push_back(rand);
  }
  time_end = std::chrono::high_resolution_clock::now();
  std::chrono::microseconds time_keygen_micro =
      std::chrono::duration_cast<std::chrono::microseconds>(time_end -
                                                            time_start);

  std::cout << "SPRING KeyGen Time: " << time_keygen_micro.count()
            << " microseconds" << std::endl;
  uint64_t *prf_out = (uint64_t *)calloc(elements.size(), sizeof(uint64_t));
  for (size_t i = 0; i < elements.size(); ++i) {
    prf_out[i] = LEAP::prf(elements[i], private_keys);
  }
  puts("[PSI] PRF done");

  // make some space in memory
  auto num_server_elements = elements.size();
  elements.clear();

  /*
   * fill Cuckoo filter
   */
  CuckooFilter cf(num_server_elements);

  for (size_t i = 0; i < num_server_elements; i++) {
    auto success = cf.Add(&prf_out[i]);
    (void)success;
    assert(success == cuckoofilter::Ok);
  }
  free(prf_out);
  prf_out = NULL;

  const uint64_t size_in_tags = cf.SizeInTags();
  const uint64_t step = (1 << 16);
  std::array<uint64_t, 3> cf_params;

  cf_params[0] = htobe64(num_server_elements);
  cf_params[1] = htobe64(size_in_tags);
  cf_params[2] = htobe64(step);
  osuCrypto::span<uint8_t> cf_param_span((uint8_t *)cf_params.data(), 3 * 8);

  coproto::sync_wait(socket.send(cf_param_span));

  for (uint64_t i = 0; i < size_in_tags; i += step) {
    std::vector<uint8_t> cf_ser = cf.serialize(step, i);
    uint64_t cfsize = cf_ser.size();
    cfsize = htobe64(cfsize);
    coproto::sync_wait(socket.send(cfsize));
    osuCrypto::span<uint8_t> tmpSpan(cf_ser);
    coproto::sync_wait(socket.send(tmpSpan));
  }

  std::vector<unsigned __int128> hash_params =
      cf.GetTwoIndependentMultiplyShiftParams();
  osuCrypto::span<unsigned __int128> hashParamSpan(hash_params);
  coproto::sync_wait(socket.send(hashParamSpan));

  auto time4 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> trans_time = time4 - time0;
  uint64_t end = socket.bytesSent() - begin;
  printf("[PSI] Setup %fs \n Setup Comm: %fMiB sent\n", trans_time.count(),
         end / 1024.0 / 1024.0);
}

void SpringNRPSIServer::Base() {
  auto time3 = std::chrono::high_resolution_clock::now();
  uint64_t begin = socket.bytesSent();

  std::vector<block> baseOTs;
  BitVector baseChoices(numBaseOTs);
  baseChoices.randomize(prng);
  baseOTs.resize(numBaseOTs);
  osuCrypto::span<block> baseOTsSpan(baseOTs.data(), baseOTs.size());

  uint64_t num_client_elements;
  coproto::sync_wait(socket.recv(num_client_elements));
  num_client_elements_ = be64toh(num_client_elements);

#ifdef KYBER_OT
  MasnyRindalKyber ot;
#else
  AsmSimplestOT ot;
#endif
  coproto::sync_wait(ot.receive(baseChoices, baseOTsSpan, prng, socket));

#ifdef IKNP
  osuCrypto::IknpOtExtSender otExtSender;
#else
  osuCrypto::SilentOtExtSender otExtSender;
  otExtSender.configure(num_client_elements_*(N * 10 + 1152), 2,1,  SilentSecType::SemiHonest);
#endif
  otExtSender.setBaseOts(baseOTsSpan, baseChoices);

  // one for subset-sum, one for each gilboa coefficient bit, one for each
  // rounding bit
  ots_.resize((N * 19) * num_client_elements_);
  osuCrypto::span<std::array<osuCrypto::block, 2>> otSpan(ots_.data(),
                                                          ots_.size());
  coproto::sync_wait(otExtSender.send(otSpan, prng, socket));

  for (size_t j = 0; j < N * num_client_elements_; ++j) {
    Subset128 r0(otSpan[j][0], &shake256);
    Subset128 r1(otSpan[j][1], &shake256);
    r_0.push_back(r0);
    r_1.push_back(r1);
  }

  ole_r0.resize(N * LOG_Q * num_client_elements_, 0);
  ole_r1.resize(N * LOG_Q * num_client_elements_, 0);
  for (size_t j = 0; j < N * LOG_Q * num_client_elements_; ++j) {
    // rejection sampling routine
    shake256.Reset(2);
    shake256.Update((uint8_t *)&ots_[num_client_elements_ * N + j][0], 128 / 8);
    shake256.Final((uint8_t *)&ole_r0[j]);
    ole_r0[j] = ole_r0[j] & 0x1ff;
    while (ole_r0[j] >= SPRING_Q) {
      shake256.Final((uint8_t *)&ole_r0[j]);
      ole_r0[j] = ole_r0[j] & 0x1ff;
    }

    shake256.Reset(2);
    shake256.Update((uint8_t *)&ots_[num_client_elements_ * N + j][1], 128 / 8);
    shake256.Final((uint8_t *)&ole_r1[j]);
    ole_r1[j] = ole_r1[j] & 0x1ff;
    while (ole_r1[j] >= SPRING_Q) {
      shake256.Final((uint8_t *)&ole_r1[j]);
      ole_r1[j] = ole_r1[j] & 0x1ff;
    }
  }

  round_r0.resize(N * num_client_elements_, 0);
  round_r1.resize(N * num_client_elements_, 0);
  for (size_t j = 0; j < N * num_client_elements_; ++j) {
    for (size_t l = 0; l < 9; ++l) {
      auto *extract =
          (uint8_t *)&ots_[num_client_elements_ * N * 10 + j * 9 + l][0];
      round_r0[j] |= (extract[0] & 1) << l;
      extract = (uint8_t *)&ots_[num_client_elements_ * N * 10 + j * 9 + l][1];
      round_r1[j] |= (extract[0] & 1) << l;
    }
  }

  auto time4 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> trans_time = time4 - time3;
  auto end = socket.bytesSent() - begin;
  printf("[PSI] Base Time:\n\t%fs\n  Base Comm: %f MiB overall\n",
         trans_time.count(), end / 1024.0 / 1024.0);
}

void SpringNRPSIServer::Online() {
  ////////////////////////////////////////////////////
  ////////////////// Subset Sum //////////////////////
  ////////////////////////////////////////////////////
  // vectors of ss_choices for PSI
  auto time3 = std::chrono::high_resolution_clock::now();
  auto begin = socket.bytesSent();
  osuCrypto::BitVector subsum_choices(num_client_elements_ * N);
  auto serverSpan = osuCrypto::span<uint8_t>(subsum_choices.data(),
                                             subsum_choices.sizeBytes());
  coproto::sync_wait(socket.recv(serverSpan));

  std::vector<Subset128> s_prime(num_client_elements_);
  std::vector<uint8_t> send_poly(128 * N * num_client_elements_, 0);
  for (size_t i = 0; i < num_client_elements_; ++i) {
    s_prime[i] = private_keys[0];
    for (size_t j = 0; j < N; ++j) {
      Subset128 randomness;
      if (subsum_choices[i * N + j]) {
        s_prime[i] = s_prime[i] - r_1[i * N + j];
        randomness = r_0[i * N + j] ^ (r_1[i * N + j] + private_keys[j + 1]);
      } else {
        s_prime[i] = s_prime[i] - r_0[i * N + j];
        randomness = r_1[i * N + j] ^ (r_0[i * N + j] + private_keys[j + 1]);
      }
      memcpy(send_poly.data() + j * N + i * N * N,
             (uint8_t *)randomness.dlog.data(), N);
    }
    uint8_t oout[N];

    s_prime[i].get_full(oout);
  }
  auto polySpan = osuCrypto::span<uint8_t>((uint8_t *)send_poly.data(),
                                           128 * N * num_client_elements_);
  coproto::sync_wait(socket.send(polySpan));
  //////////////////////////////////////////////////
  //////////////// OLE    //////////////////////////
  //////////////////////////////////////////////////

  std::vector<uint16_t> blinder(N * num_client_elements_, 0);
  std::vector<uint16_t> ole_enc(1152 * num_client_elements_, 0);

  BitVector ole_corr(1152 * num_client_elements_);
  auto oleSpan =
      osuCrypto::span<uint8_t>(ole_corr.data(), ole_corr.sizeBytes());
  coproto::sync_wait(socket.recv(oleSpan));

  size_t idx = 0;
  for (size_t i = 0; i < num_client_elements_; ++i) {
    std::array<uint16_t, N> ole_in{};
    s_prime[i].get_int_from_subsetsum(ole_in.data());
    for (size_t j = 0; j < N; ++j) {
      uint64_t blinding_factor = 0;
      for (size_t k = 0; k < LOG_Q; k++) {
        if (ole_corr[idx]) {
          blinding_factor += ((1 << k) * ole_r1[idx]);
          ole_enc[idx] =
              ((1 << k) * (uint32_t)ole_r0[idx]) ^
              ((1 << k) * ((uint32_t)ole_in[j] + (uint32_t)ole_r1[idx]));
        } else {
          blinding_factor += ((1 << k) * ole_r0[idx]);
          ole_enc[idx] =
              ((1 << k) * (uint32_t)ole_r1[idx]) ^
              ((1 << k) * ((uint32_t)ole_in[j] + (uint32_t)ole_r0[idx]));
        }
        ++idx;
      }
      blinder[N * i + j] = (blinding_factor + SPRING_Q) % SPRING_Q;
    }
  }

  coproto::sync_wait(socket.send(ole_enc));

  uint16_t *blinder_ptr = blinder.data();
  for (auto i = 0; i < num_client_elements_; ++i) {
    intt(blinder_ptr);
    blinder_ptr += N;
  }

  ////////////////////////////////////////////////////
  ////////////////// Rounding    /////////////////////
  ////////////////////////////////////////////////////
  BitVector round_corr(num_client_elements_ * 1152);
  auto roundSpan =
      osuCrypto::span<uint8_t>(round_corr.data(), round_corr.sizeBytes());
  coproto::sync_wait(socket.recv(roundSpan));

  // 128 poly coefficients, for each we have 257 options: (128*257)/64=257*2

  std::vector<uint64_t> mask(2 * num_client_elements_, 0);
  std::vector<uint64_t> enc_bits(num_client_elements_ * 257 * 2, 0);
  for (size_t i = 0; i < num_client_elements_; ++i) {
    for (int32_t k = 0; k < SPRING_Q; k++) {
      enc_bits[i * 2 * SPRING_Q + 2 * k] = mask[i * 2];
      for (size_t j = 0; j < 64; ++j) {
        uint8_t bit = 0;
        // compute assumed rounding value
        int32_t to_round = k - (int32_t)blinder[i * N + j];
        while (to_round < 0) {
          to_round += 257;
        }
        while (to_round >= 257) {
          to_round -= 257;
        }
        if ((to_round > 63) && (to_round < 192)) {
          bit ^= 1;
        }
        // encrypt result
        for (size_t l = 0; l < 9; ++l) {
          if (round_corr[i * N * LOG_Q + j * 9 + l]) {
            // switch
            if ((k >> (LOG_Q - l - 1)) & 1) {
              bit ^= ((round_r0[i * N + j] >> l) & 1);
            } else {
              bit ^= ((round_r1[i * N + j] >> l) & 1);
            }
          } else {
            if ((k >> (LOG_Q - l - 1)) & 1) {
              bit ^= ((round_r1[i * N + j] >> l) & 1);
            } else {
              bit ^= ((round_r0[i * N + j] >> l) & 1);
            }
          }
        }
        enc_bits[i * 2 * SPRING_Q + 2 * k] |= ((uint64_t)bit << j);
      }
      enc_bits[i * 2 * SPRING_Q + 2 * k + 1] = mask[i * 2 + 1];
      for (size_t j = 64; j < N; ++j) {
        uint8_t bit = 0;
        // compute assumed rounding value
        int32_t to_round = k - (int32_t)blinder[i * N + j];
        while (to_round < 0) {
          to_round += 257;
        }
        while (to_round >= 257) {
          to_round -= 257;
        }
        if ((to_round > 63) && (to_round < 192))
          bit ^= 1;
        for (size_t l = 0; l < 9; ++l) {
          if (round_corr[i * N * 9 + j * 9 + l]) {
            // switch
            if ((k >> (LOG_Q - l - 1)) & 1) {
              bit ^= ((round_r0[i * N + j] >> l) & 1);
            } else {
              bit ^= ((round_r1[i * N + j] >> l) & 1);
            }
          } else {
            if ((k >> (LOG_Q - l - 1)) & 1) {
              bit ^= ((round_r1[i * N + j] >> l) & 1);
            } else {
              bit ^= ((round_r0[i * N + j] >> l) & 1);
            }
          }
        }
        enc_bits[i * 2 * SPRING_Q + 2 * k + 1] |= ((uint64_t)bit << (j - 64));
      }
    }
  }

  coproto::sync_wait(socket.send(enc_bits));

  std::vector<uint64_t> bch_mask(num_client_elements_, 0);
  uint64_t *mask_ptr = mask.data();
  for (size_t i = 0; i < num_client_elements_; ++i) {
    bch_mask[i] = BCH128to64(mask_ptr);
    mask_ptr += 2;
  }
  coproto::sync_wait(socket.send(bch_mask));

  auto time4 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> trans_time = time4 - time3;
  auto end = socket.bytesSent() - begin;
  printf("[PSI] Online Time:\n\t%fs\n  Online Comm: %f MiB overall\n",
         trans_time.count(), end / 1024.0 / 1024.0);
}
} // namespace LEAP
