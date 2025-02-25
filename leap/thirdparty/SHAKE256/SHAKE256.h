#pragma once
// This file and the associated implementation has been placed in the public
// domain, waiving all copyright. No restrictions are placed on its use.
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

extern "C" {
#include "keccak/KeccakHash.h"
#include "keccak/KeccakP-1600-SnP.h"
// #include <keccak/KeccakHash.h>
}

// namespace droidCrypto {

// An implementation of SHA1 based on ARM NEON instructions
class SHAKE256 {
public:
  // Default constructor of the class. Sets the internal state to zero.
  SHAKE256(uint64_t outputLength = 64) { Reset(outputLength); }

  // Resets the interal state.
  void Reset() { Reset(outputLength); }

  // Resets the interal state and sets the desired output length in bytes.
  void Reset(uint64_t digestByteLength) {
    Keccak_HashInitialize_SHAKE256(&ctx);
    outputLength = digestByteLength;
  }

  // Add length bytes pointed to by dataIn to the internal SHA1 state.
  template <typename T>
  typename std::enable_if<std::is_pod<T>::value>::type Update(const T *dd,
                                                              uint64_t ll) {
    auto length = ll * sizeof(T);
    uint8_t *dataIn = (uint8_t *)dd;

    Keccak_HashUpdate(&ctx, dataIn, length * 8);
  }
  template <typename T>
  typename std::enable_if<std::is_pod<T>::value>::type Update(const T &blk) {
    Update((uint8_t *)&blk, sizeof(T));
  }

  // Finalize the SHAKE256 digest and output the result to DataOut.
  // Required: DataOut must be at least SHAKE256::outputLength in length.
  void Final(uint8_t *DataOut) {
    Keccak_HashFinal(&ctx, NULL);
    Keccak_HashSqueeze(&ctx, DataOut, outputLength * 8);
  }

  // Copy the interal state of a SHA1 computation.
  SHAKE256 &operator=(const SHAKE256 &src);

  uint64_t getOutputLength() const { return outputLength; }

private:
  Keccak_HashInstance ctx;
  uint32_t outputLength;
};
//}  // namespace droidCrypto
