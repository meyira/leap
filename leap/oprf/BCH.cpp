#include <leap/oprf/BCH.h>

extern uint64_t BCH128to64(uint64_t *packedLSBbits) {
  uint64_t b1 = packedLSBbits[0];
  uint64_t res = b1;
  res ^= (b1 << 2);
  res ^= (b1 << 7);
  res ^= (b1 << 8);
  res ^= (b1 << 10);
  res ^= (b1 << 12);
  res ^= (b1 << 14);
  res ^= (b1 << 15);
  res ^= (b1 << 16);
  res ^= (b1 << 23);
  res ^= (b1 << 25);
  res ^= (b1 << 27);
  res ^= (b1 << 28);
  res ^= (b1 << 30);
  res ^= (b1 << 31);
  res ^= (b1 << 32);
  res ^= (b1 << 33);
  res ^= (b1 << 37);
  res ^= (b1 << 38);
  res ^= (b1 << 39);
  res ^= (b1 << 40);
  res ^= (b1 << 41);
  res ^= (b1 << 42);
  res ^= (b1 << 44);
  res ^= (b1 << 45);
  res ^= (b1 << 48);
  res ^= (b1 << 58);
  res ^= (b1 << 61);
  res ^= (b1 << 63);

  uint64_t b2 = packedLSBbits[1];
  res ^= (b2 >> 62);
  res ^= (b2 >> 57);
  res ^= (b2 >> 56);
  res ^= (b2 >> 54);
  res ^= (b2 >> 52);
  res ^= (b2 >> 50);
  res ^= (b2 >> 49);
  res ^= (b2 >> 48);
  res ^= (b2 >> 41);
  res ^= (b2 >> 39);
  res ^= (b2 >> 37);
  res ^= (b2 >> 36);
  res ^= (b2 >> 34);
  res ^= (b2 >> 33);
  res ^= (b2 >> 32);
  res ^= (b2 >> 31);
  res ^= (b2 >> 27);
  res ^= (b2 >> 26);
  res ^= (b2 >> 25);
  res ^= (b2 >> 24);
  res ^= (b2 >> 23);
  res ^= (b2 >> 22);
  res ^= (b2 >> 20);
  res ^= (b2 >> 19);
  res ^= (b2 >> 16);
  res ^= (b2 >> 6);
  res ^= (b2 >> 3);
  res ^= (b2 >> 1);

  return res ^ ((uint64_t)(-(b2 & 1)));
}

//  // EQUIVALENCE TEST FOR VERIFICATION
//  int main(void){
//    uint64_t server[2]={0x31f50d17f29066d6, 0x936bb3c0974837a5};
//    uint64_t client[2]={0x15e50f87d2f4e6d7, 0x932b19c196493fa5};
//    uint64_t sanity[2]={0xc8684d2b51dd8722, 0x9493548e67ae9452};
//    for(size_t i=0; i<2; ++i){
//      server[i]=rand();
//      client[i]=rand();
//      sanity[i]=server[i]^client[i];
//    }
//    uint64_t result=BCH128to64(sanity);
//    uint64_t masked=BCH128to64(server);
//    uint64_t masked_result=BCH128to64(client);
//
//    uint64_t result2=masked^masked_result;
//
//    printf("%d %0lx %0lx\n", result==result2, result, result2);
//
//    return 0;
//  }
