#ifndef PRECOMPUTE_SPRING_H
#define PRECOMPUTE_SPRING_H

#include "params.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// static int8_t A[K+1][N];
// static int16_t D[K+1][2][N]; // D[k][1] is the inverse
// static int16_t root_of_unity[7][64];
// static int16_t roots[7];
// static int16_t inv_root_of_unity[7][64];

void precomputeRoots();
void calculateTableA();
void calculateTableD();
void precomputeAll();

#ifdef __cplusplus
}
#endif

#endif // end PRECOMPUTE_SPRING_H
