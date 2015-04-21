#include <rune.h>
static const unsigned char biti_32v[32] = {
   0, 1,28, 2,29,14,24, 3,	30,22,20,15,25,17, 4, 8,
  31,27,13,23,21,19,16, 7,	26,12,18, 6,11, 5,10, 9};
static const Rune biti_32m = 0x077cb531;
int log2rune(unsigned r) {
  unsigned i = r;
  i |= i >> 1; i |= i >> 2; i |= i >> 4;
  i |= i >> 8; i |= i >> 16;
  i=i/2+1;
  return biti_32v[(Rune)(i * biti_32m) >> 27];
}
