#include <leap/psi/SpringNRPSIClient.h>

namespace LEAP {
SpringNRPSIClient::SpringNRPSIClient(cp::AsioSocket &sock)
    // role 2, num_threads 1
    : cf_(nullptr), prng(sysRandomSeed()) {
  this->socket = sock;
}

void SpringNRPSIClient::Setup() {
  /*
   * OT SETUP
   */
  std::chrono::high_resolution_clock::time_point time_start, time_end;
  time_start = std::chrono::high_resolution_clock::now();
  auto begin = socket.bytesSent();

  /*
   * receive and parse Cuckoo filter
   */

  std::array<uint64_t, 3> cf_params;
  osuCrypto::span<uint8_t> cf_param_span((uint8_t *)cf_params.data(), 3 * 8);
  coproto::sync_wait(socket.recv(cf_param_span));

  uint64_t size_in_tags, step, num_server_elements;
  num_server_elements = be64toh(cf_params[0]);
  size_in_tags = be64toh(cf_params[1]);
  step = be64toh(cf_params[2]);

  cf_ = new CuckooFilter(num_server_elements);

  for (uint64_t i = 0; i < size_in_tags; i += step) {
    std::vector<uint8_t> tmp;
    uint64_t cfsize;
    coproto::sync_wait(socket.recv(cfsize));
    cfsize = be64toh(cfsize);
    tmp.resize(cfsize, 0);
    osuCrypto::span<uint8_t> tmpSpan(tmp);
    coproto::sync_wait(socket.recv(tmpSpan));
    cf_->deserialize(tmp, i);
  }
  std::vector<unsigned __int128> params(3);
  osuCrypto::span<unsigned __int128> paramSpan(params);
  coproto::sync_wait(socket.recv(paramSpan));
  cf_->SetTwoIndependentMultiplyShiftParams(params);
  time_end = std::chrono::high_resolution_clock::now();
  uint64_t end = socket.bytesSent() - begin;
  std::chrono::duration<double> trans = time_end - time_start;
  printf("[PSI] Time: %fs\n Size: %f MiB\n", trans.count(),
         end / 1024.0 / 1024.0);
}

void SpringNRPSIClient::Base(size_t num_elements) {
  /*
   * request OT's
   */
  // network order
  auto time3 = std::chrono::high_resolution_clock::now();
  auto begin = socket.bytesSent();

  std::vector<std::array<block, 2>> baseOTs;
  baseOTs.resize(numBaseOTs);
  osuCrypto::span<std::array<block, 2>> baseOTsSpan(baseOTs.data(),
                                                    baseOTs.size());

  uint64_t num_client_elements = htobe64(num_elements);
  coproto::sync_wait(socket.send(num_client_elements));

#ifdef KYBER_OT
  MasnyRindalKyber ot;
#else
  AsmSimplestOT ot;
#endif
  coproto::sync_wait(ot.send(baseOTsSpan, prng, socket));

#ifdef IKNP
  IknpOtExtReceiver OTeRecv;
#else
  SilentOtExtReceiver OTeRecv;
  OTeRecv.configure(num_elements*(N * 10 + 1152), 2, 1, SilentSecType::SemiHonest);
#endif
  OTeRecv.setBaseOts(baseOTsSpan);

  ot_choices_.resize((N * 19) * num_elements);
  ot_choices_.randomize(prng);
  ots_.resize(ot_choices_.size());
  osuCrypto::span<osuCrypto::block> otSpan(ots_.data(), ots_.size());
  coproto::sync_wait(OTeRecv.receive(ot_choices_, otSpan, prng, socket));

  for (size_t j = 0; j < num_elements * N; ++j) {
    Subset128 privkey(otSpan[j], &shake256);
    ot_ss.push_back(privkey);
  }

  ole_int.resize(num_elements * N * LOG_Q, 0);

  for (size_t j = 0; j < num_elements * N * LOG_Q; ++j) {
    // rejection sampling routine
    shake256.Reset(2);
    shake256.Update((uint8_t *)&ots_[num_elements * N + j], 128 / 8);
    shake256.Final((uint8_t *)&ole_int[j]);
    ole_int[j] = ole_int[j] & 0x1ff;
    while (ole_int[j] >= SPRING_Q) {
      shake256.Final((uint8_t *)&ole_int[j]);
      ole_int[j] = ole_int[j] & 0x1ff;
    }
  }
  round_r.resize(N * num_elements);

  for (size_t j = 0; j < num_elements * N; ++j) {
    round_r[j] = 0;
    for (size_t l = 0; l < 9; ++l) {
      auto *extract = (uint8_t *)&ots_[num_elements * N * 10 + j * 9 + l];
      round_r[j] ^= (extract[0] & 1);
    }
  }

  auto time4 = std::chrono::high_resolution_clock::now();
  auto end = socket.bytesSent() - begin;
  std::chrono::duration<double> trans_time = time4 - time3;
  printf("[PSI] Base Phase Time:\n\t%fs\n Base Comm: %fMiB\n",
         trans_time.count(), end / 1024.0 / 1024.0);
}

std::vector<size_t> SpringNRPSIClient::Online(std::vector<block> &elements) {
  auto begin = socket.bytesSent();
  auto time4 = std::chrono::high_resolution_clock::now();

  // allocate space for unblinding polynomials
  std::vector<Subset128> aggregated(elements.size());

  // initialize OT correction step
  BitVector server_bv;
  server_bv.copy(ot_choices_, 0, N * elements.size());
  std::vector<uint8_t> received(elements.size() * 128 * N, 0);
  osuCrypto::BitVector bv;
  for (size_t i = 0; i < elements.size(); ++i) {
    // convert inputs to bitvector
    BitVector b;
    b.assign(elements[i]);
    bv.append(b, 128, 0);
  }
  server_bv = server_bv ^ bv;

  auto serverBvSpan =
      osuCrypto::span<uint8_t>(server_bv.data(), server_bv.sizeBytes());
  auto recvSpan = osuCrypto::span<uint8_t>((uint8_t *)received.data(),
                                           elements.size() * 128 * N);

  coproto::sync_wait(socket.send(serverBvSpan));
  coproto::sync_wait(socket.recv(recvSpan));
  uint8_t *r_ptr = received.data();
  for (size_t i = 0; i < elements.size(); ++i) {
    for (size_t j = 0; j < N; ++j) {
      Subset128 r(r_ptr);
      if (bv[i * N + j]) {
        aggregated[i] = aggregated[i] + (ot_ss[i * N + j] ^ r);
      } else {
        aggregated[i] = aggregated[i] + ot_ss[i * N + j];
      }
      r_ptr += N;
    }
    // copy AVX revister data used for addition to uint8_t array
    uint8_t oout[N];
    aggregated[i].get_full(oout);
  }

  //////////////////////////////////////////////////
  //////////////// Client OLE  ///////////////////////
  ////////////////////////////////////////////////////
  BitVector ole_choices;
  ole_choices.copy(ot_choices_, elements.size() * N, elements.size() * N * 9);

  BitVector ole_corr(N * 9 * elements.size());

  size_t idx = 0;
  for (size_t i = 0; i < elements.size(); ++i) {
    std::array<uint16_t, N> ole_in{};
    // convert to subset-product
    aggregated[i].get_int_from_subsetsum(ole_in.data());
    for (unsigned short j : ole_in) {
      for (size_t k = 0; k < 9; ++k) {
        // becomes true if any bit is set
        ole_corr[idx] = (j & (1 << k)) >> k;
        ++idx;
      }
    }
  }
  ole_choices = ole_corr ^ ole_choices;
  auto oleSpan =
      osuCrypto::span<uint8_t>(ole_choices.data(), ole_choices.sizeBytes());
  coproto::sync_wait(socket.send(oleSpan));

  std::vector<uint16_t> ole_enc(1152 * elements.size(), 0);
  coproto::sync_wait(socket.recv(ole_enc));
  std::vector<uint16_t> y(N * elements.size(), 0);
  idx = 0;
  for (unsigned short &j : y) {
    uint32_t result = 0;
    for (size_t k = 0; k < LOG_Q; ++k) {
      if (ole_corr[idx]) {
        result = result +
                 (((1 << k) * (uint32_t)ole_int[idx]) ^ (uint32_t)ole_enc[idx]);
      } else {
        result = result + ((1 << k) * (uint32_t)ole_int[idx]);
      }
      ++idx;
    }
    j = (result + SPRING_Q) % SPRING_Q;
  }

  // perform inverse NTT
  uint16_t *y_ptr = y.data();
  for (size_t j = 0; j < elements.size(); ++j) {
    intt(y_ptr);
    y_ptr += N;
  }

  ////////////////////////////////////////////////////
  ////////////////// Client Rounding  ////////////////
  ////////////////////////////////////////////////////

  std::vector<block> recvMsgs(elements.size() * N);
  BitVector rounding_corr(elements.size() * 1152);
  idx = 0;
  for (unsigned short j : y) {
    for (int64_t k = 8; k >= 0; --k) {
      rounding_corr[idx] = (j & (1 << k)) >> k;
      ++idx;
    }
  }
  BitVector rounding_choices;
  rounding_choices.copy(ot_choices_, elements.size() * N * 10,
                        elements.size() * 1152);
  // XOR with choice bits
  rounding_choices = rounding_corr ^ rounding_choices;
  auto roundSpan = coproto::span<uint8_t>(rounding_choices.data(),
                                          rounding_choices.sizeBytes());
  std::vector<uint64_t> enc_bits(elements.size() * 2 * 257, 0);
  std::vector<uint64_t> mask(elements.size(), 0);
  std::vector<uint64_t> prf_out(elements.size());
#ifdef LEAP_MICRO_BENCHMARKS
  auto cc_rounding_wait = time.setTimePoint("round-wait");
#endif
  coproto::sync_wait(socket.send(roundSpan));
  coproto::sync_wait(socket.recv(enc_bits));
  coproto::sync_wait(socket.recv(mask));
#ifdef LEAP_MICRO_BENCHMARKS
  auto cc_round = time.setTimePoint("round");
#endif

  for (size_t i = 0; i < elements.size(); ++i) {
    uint64_t biased[2] = {0};
    for (size_t j = 0; j < 64; ++j) {
      biased[0] ^= (((enc_bits[y[i * N + j] * 2 + 2 * i * SPRING_Q] >> j) & 1) ^
                    round_r[j + i * N])
                   << j;
    }
    for (size_t j = 64; j < N; ++j) {
      biased[1] |=
          (((enc_bits[y[i * N + j] * 2 + 2 * i * SPRING_Q + 1] >> (j - 64)) &
            1) ^
           round_r[j + i * N])
          << (j - 64);
    }
    prf_out[i] = mask[i] ^ BCH128to64(biased);
  }
  std::vector<uint64_t> res;
  /*
   * finalize and do intersection
   */
  for (size_t i = 0; i < elements.size(); ++i) {
    if (cf_->Contain(&prf_out[i]) == cuckoofilter::Ok) {
      res.push_back(i);
    }
  }

  auto time6 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> recv = time6 - time4;
  auto end = socket.bytesSent() - begin;
  printf("[PSI] Online Time: %fs\n\t Online Comm %f MiB\n", recv.count(),
         end / 1024.0 / 1024.0);

  return res;
}

} // namespace LEAP
