#include <leap/oprf/ntt-falcon.h>

namespace LEAP {
// a table of the powers of omega.     i --> omega**(2N-i)             ( mod
// FIELD_SIZE )
const uint16_t phi4_roots_Zq[] = {16, 241};

//"""Roots of phi_8 = x^4 + 1"""
const uint16_t phi8_roots_Zq[] = {4, 253, 64, 193};

//"""Roots of phi_16 = x^8 + 1"""
const uint16_t phi16_roots_Zq[] = {2, 255, 32, 225, 8, 249, 128, 129};

//"""Roots of phi_32 = x^16 + 1"""
const uint16_t phi32_roots_Zq[] = {60,  197, 68,  189, 17, 240, 15, 242,
                                   120, 137, 121, 136, 34, 223, 30, 227};

//"""Roots of phi_64 = x^32 + 1"""
const uint16_t phi64_roots_Zq[] = {
    46, 211, 35, 222, 117, 140, 73, 184, 70, 187, 92,  165, 23, 234, 111, 146,
    67, 190, 44, 213, 11,  246, 81, 176, 88, 169, 123, 134, 95, 162, 22,  235};

//"""Roots of phi_128 = x^64 + 1"""
const uint16_t phi128_roots_Zq[] = {
    118, 139, 89,  168, 99,  158, 42,  215, 84,  173, 59,  198, 79,
    178, 21,  236, 29,  228, 50,  207, 116, 141, 57,  200, 58,  199,
    100, 157, 25,  232, 114, 143, 18,  239, 31,  226, 72,  185, 124,
    133, 36,  221, 62,  195, 9,   248, 113, 144, 49,  208, 13,  244,
    61,  196, 52,  205, 98,  159, 26,  231, 104, 153, 122, 135};

//"""Roots of phi_256 = x^128 + 1"""
const uint16_t phi256_roots_Zq[] = {
    115, 142, 41,  216, 54,  203, 93,  164, 108, 149, 71,  186, 82,  175, 27,
    230, 37,  220, 78,  179, 109, 148, 55,  202, 74,  183, 101, 156, 110, 147,
    39,  218, 85,  172, 75,  182, 43,  214, 83,  174, 87,  170, 107, 150, 91,
    166, 86,  171, 40,  217, 126, 131, 10,  247, 97,  160, 5,   252, 80,  177,
    63,  194, 20,  237, 77,  180, 53,  204, 51,  206, 45,  212, 103, 154, 106,
    151, 102, 155, 90,  167, 6,   251, 96,  161, 24,  233, 127, 130, 3,   254,
    48,  209, 65,  192, 12,  245, 7,   250, 112, 145, 28,  229, 66,  191, 33,
    224, 14,  243, 56,  201, 125, 132, 94,  163, 38,  219, 119, 138, 105, 152,
    19,  238, 47,  210, 76,  181, 69,  188};

const uint16_t *roots_dict[7] = {
    phi4_roots_Zq,  phi8_roots_Zq,   phi16_roots_Zq, phi32_roots_Zq,
    phi64_roots_Zq, phi128_roots_Zq, phi256_roots_Zq};

/////////////////////////////////////////////////
/////////////     begin NTT   ///////////////////
/////////////////////////////////////////////////

// void split(int16_t in[],int16_t x_even[],int16_t x_odd[], size_t new_size){
//     for(size_t i=0; i<new_size; ++i){
//         x_even[i]=in[i*2];
//         x_odd[i]=in[(i*2)+1];
//     }
// }

void split_ntt(uint16_t in[], uint16_t x_even[], uint16_t x_odd[],
               size_t new_size) {
  int32_t idx = 32 - __builtin_clz(new_size) - 1;
  const uint16_t *table = roots_dict[idx];
  for (size_t i = 0; i < new_size; ++i) {
    x_even[i] = ((uint32_t)ii2 * (in[i * 2] + in[2 * i + 1])) % SPRING_Q;
    // TODO make this better, problem is that mod has the sign bit thing and
    // goes off by one
    int16_t tmp =
        ((ii2 * ((in[i * 2] - in[2 * i + 1])) * inv_mod_q[table[2 * i]])) %
        SPRING_Q;
    if (tmp < 0)
      x_odd[i] = tmp + 257;
    else
      x_odd[i] = (uint16_t)tmp;
  }
}

void merge(uint16_t out[], uint16_t in1[], uint16_t in2[], size_t size) {
  for (size_t i = 0; i < size / 2; ++i) {
    out[2 * i + 0] = in1[i];
    out[2 * i + 1] = in2[i];
  }
}
// void merge_ntt(int16_t out[],int16_t in1[],int16_t in2[], size_t size){
//     size_t idx;
//     if(size==4)
//         idx=0;
//     else if(size==8)
//         idx=1;
//     else if(size==16)
//         idx=2;
//     else if(size==32)
//         idx=3;
//     else if(size==64)
//         idx=4;
//     else if(size==128)
//         idx=5;
//     else // (new_size==256)
//         idx=6;
//     const uint16_t *table=roots_dict[idx];
//     for(size_t i=0; i<size/2; ++i){
//         // TODO HERE IS AN ERROR, probably alsoi n the mod
//         out[2*i]=(in1[i]+table[idx+2*i]*in2[i])%SPRING_Q;
//         out[2*i+1]=(in1[i]-table[idx+2*i]*in2[i])%SPRING_Q;
//     }
// }

// void ntt(int16_t x[], size_t size){
//     /*
//      * general-purpose recursive ntt
//      * @x input polynomial
//      * @size is the size of the x-array
//      * @step needed for lookup table of n-th root
//      * of unity
//      */
//     if(size>2){
//         /*
//          * split [0,1,2,3]into even [0,2] and odd [1,3]
//          */
//         size_t new_size=size/2;
//         int16_t x_even[new_size];
//         int16_t x_odd[new_size];

//         split(x, x_even, x_odd, new_size);
//         ntt(x_even, new_size);
//         ntt(x_odd, new_size);

//         merge_ntt(x, x_even, x_odd, size);
//         for(size_t i=0; i<size; ++i){
//             if(x[i]<0)
//                 x[i]+=SPRING_Q;
//         }
//     }
//     else if(size==2){
//         int16_t factor=sqr1*x[1];
//         x[1]=(x[0]-factor)%SPRING_Q;
//         if(x[1]<0)
//             x[1]+=SPRING_Q;
//         x[0]=(x[0]+factor)%SPRING_Q;
//         if(x[0]<0)
//             x[0]+=SPRING_Q;
//     }
// }

void intt(uint16_t x[], size_t size) {
  /*
   * general-purpose recursive inverse ntt
   * @x input polynomial
   * @size is the size of the x-array
   * @step needed for lookup table of n-th root
   * of unity
   */
  if (size > 2) {
    /*
     * split [0,1,2,3]into even [0,2] and odd [1,3]
     */
    size_t new_size = size / 2;
    uint16_t x_even[new_size];
    uint16_t x_odd[new_size];
    split_ntt(x, x_even, x_odd, new_size);

    intt(x_even, new_size);
    intt(x_odd, new_size);

    merge(x, x_even, x_odd, size);

  } else if (size == 2) {
    // save x[0] for later step
    uint16_t x0 = x[0];
    x[0] = ((uint32_t)ii2 * (x[0] + x[1])) % SPRING_Q;
    // TODO make this better, problem is that mod has the sign bit thing and
    // goes off by one
    int16_t tmp = ((ii2 * inv_mod_q[16] * (x0 - x[1]))) % SPRING_Q;
    if (tmp < 0)
      x[1] = tmp + 257;
    else
      x[1] = tmp;
  }
}
/////////////////////////////////////////////////
//////////    NTT arithmetic   //////////////////
/////////////////////////////////////////////////
void add(int16_t result[], int16_t a[], int16_t b[]) {
  for (size_t i = 0; i < N; ++i)
    result[i] = ((int32_t)a[i] + b[i]) % SPRING_Q;
}
void subtract(int16_t result[], int16_t a[], int16_t b[]) {
  for (size_t i = 0; i < N; ++i)
    result[i] = ((int32_t)a[i] - b[i]) % SPRING_Q;
}
void multiply(int16_t result[], int16_t a[], int16_t b[]) {
  for (size_t i = 0; i < N; ++i)
    result[i] = ((int32_t)a[i] * b[i]) % SPRING_Q;
}
void divide(int16_t result[], int16_t a[], int16_t b[]) {
  for (size_t i = 0; i < N; ++i) {
    if (a[i] == 0 || b[i] == 0)
      throw std::runtime_error("Attempted Division by Zero");
    if (b[i] < 0)
      result[i] = ((int32_t)a[i] * inv_mod_q[b[i] + SPRING_Q]) % SPRING_Q;
    else
      result[i] = ((int32_t)a[i] * inv_mod_q[b[i]]) % SPRING_Q;
  }
}

} // namespace LEAP
