//  #include <droidCrypto/SHAKE256.h>
#include "SHAKE256.h"

SHAKE256 &SHAKE256::operator=(const SHAKE256 &src) {
  this->outputLength = src.outputLength;
  memcpy(&this->ctx, &src.ctx, sizeof(Keccak_HashInstance));
  return *this;
}
