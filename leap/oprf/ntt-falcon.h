#ifndef FALCON_NTT_H
#define FALCON_NTT_H
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <leap/oprf/params.h>
#include <stdexcept>
namespace LEAP {
#define sqr1 16

void intt(uint16_t x[], size_t size = 128);
// for testing
void ntt(int16_t x[], size_t size = 128);

void add(int16_t result[], int16_t a[], int16_t b[]);
void subtract(int16_t result[], int16_t a[], int16_t b[]);
void multiply(int16_t result[], int16_t a[], int16_t b[]);
void divide(int16_t result[], int16_t a[], int16_t b[]);
} // namespace LEAP
#endif
