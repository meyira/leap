#include "precomputations.h"
#include "randarr.h"
#include <stddef.h>

int16_t invmod(int16_t a, size_t b) {
  // based on https://rosettacode.org/wiki/Modular_inverse#C.2B.2B
  int16_t b0 = b, t, q;
  int16_t x0 = 0, x1 = 1;
  if (b == 1)
    return 1;
  while (a > 1) {
    q = a / b;
    t = b, b = a % b, a = t;
    t = x0, x0 = x1 - q * x0, x1 = t;
  }
  if (x1 < 0)
    x1 += b0;
  return x1;
}

void precomputeRoots() {
  size_t number_roots = 7;
  size_t nth_root = 9;

  root_of_unity[0][0] = 1;
  inv_root_of_unity[0][0] = 1;

  int16_t inv_root = invmod(nth_root, 257);

  for (size_t i = 1; i < 64; ++i) {
    root_of_unity[0][i] = (root_of_unity[0][i - 1] * nth_root) % 257;
    inv_root_of_unity[0][i] = (inv_root_of_unity[0][i - 1] * inv_root) % 257;
  }
  for (size_t i = 1; i < number_roots; ++i) {
    // x^0 is always 1
    root_of_unity[i][0] = 1;
    // compute next root by sqaring previous
    root_of_unity[i][1] =
        (root_of_unity[i - 1][1] * root_of_unity[i - 1][1]) % 257;
    // compute inverse
    inv_root_of_unity[i][0] = 1;
    inv_root_of_unity[i][1] = invmod(root_of_unity[i][1], 257);
    for (size_t j = 2; j < 64; ++j) {
      // compute root^j for to save later computations
      root_of_unity[i][j] =
          (root_of_unity[i][j - 1] * root_of_unity[i][1]) % 257;
      // inv_root_of_unity[i][j]=(inv_root_of_unity[i][j-1]*inv_root_of_unity[i][1])%257;
      inv_root_of_unity[i][j] = invmod(root_of_unity[i][j], 257);
    }
  }
}

void calculateTableA() {
  // generate secret key
  size_t r = 0;
  for (size_t i = 0; i < K; ++i) {
    for (size_t j = 0; j < N; ++j) {
      // random number in [0,255]
      A[i][j] = _randGeneratedNumbersForRq[r];
      ++r;
      // A[i][j]=rand()%256;
    }
    // ntt128(A[i]);
  }
  /*
   * always present in multiplication, adds exactly one LOG_OF_INV_N to the
   * subset sum, which is important for the DFT but could be omitted (from
   * SPRING source
   */
  for (size_t j = 0; j < N; ++j) {
    A[K][j] = _randGeneratedNumbersForRq[r];
    A[K][j] = (A[K][j] + LOG_INV_N) % 257;
    ++r;
  }
  // ntt128(A[K]);
}
void calculateTableD() {
  for (size_t i = 0; i < K + 1; ++i) {
    for (size_t j = 0; j < N; ++j) {
      // seed element
      int16_t idx = A[i][j];
      if (idx < 0)
        idx += 256;
      D[i][0][j] = generatorPowers[idx];
      // inverse seed element
      D[i][1][j] = generatorPowers[(256 - idx) % 256];
    }
  }
}

void precomputeAll() {
  calculateTableA();
  calculateTableD();
  precomputeRoots();
}
