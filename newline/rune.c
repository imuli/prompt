#include <string.h>
#include <stdio.h>
#include "rune.h"

static const unsigned int biti_32v[32] = {
   0, 1,28, 2,29,14,24, 3,	30,22,20,15,25,17, 4, 8,
  31,27,13,23,21,19,16, 7,	26,12,18, 6,11, 5,10, 9};
static const unsigned long biti_32m = 0x077cb531;
static int
log2rune(unsigned r) {
  unsigned i = r;
  i |= i >> 1; i |= i >> 2; i |= i >> 4;
  i |= i >> 8; i |= i >> 16;
  i=i/2+1;
  return biti_32v[(Rune)(i * biti_32m) >> 27];
}

int
runechar(Rune r){
	int i = log2rune(r);
	if(i<7) return 1;
	return (i-1)/5+1;
}

int
runeschars(Runes r){
	int n = 0, i;
	for(i=r->c;i>0;i--){
		n+= runechar(r->r[i]);
	}
	return n;
}

int
utf8char(char *u){
	int i = log2rune(0xff&~*u);
	if(i>5) return i-6;
	return 7-i;
}

int
utf8srunes(char *u){
	int n, i = 0;
	for(;*u!=0;u+=n){
		n = utf8char(u);
		if(n < 1) return -1;
		i++;
	}
	return i;
}

int
rune_utf8(Rune *rr, char *u){
	unsigned long mask;
	int n = utf8char(u);
	int i;
	Rune r;
	/* check for spurious 10xxxxxx bytes */
	if(n == 0){
		*rr = BadRune;
		return 1;
	}
	mask = 0xff >> n;
	r = *u & mask;
	for(i=1;i<n;i++){
		/* check for complete characters */
		if((u[i] & 0xc0) != 0x80){
			*rr = IncRune;
			return i;
		}
		r <<= 6;
		r |= u[i] & 0x3f;
	}
	/* enforce minimum length utf8 encoding */
	if(i != runechar(r))
		r = BadRune;
	*rr = r;
	return i;
}

int
runes_utf8s(Runes r, char *u){
	int n = 0, i = r->c;
	for(;u[n]!=0;i++){
		n += rune_utf8(r->r+i, u+n);
	}
	r->c = i;
	return i;
}

int
utf8_rune(char *u, Rune r){
	unsigned long mask;
	int n = runechar(r);
	int i;
	mask = n==1 ? 0 : 0xff00 >> n;
	u[n] = '\0';
	for(i=n-1;i>0;i--){
		u[i] = 0x80 | (r & 0x3f);
		r >>= 6;
	}
	u[0] = mask | r;
	return n;
}

int
utf8s_runes(char *u, Runes r){
	int n = 0, i;
	for(i=r->c;i>0;i--){
		n += utf8_rune(u+n, r->r[i]);
	}
	*u = '\0';
	return n;
}

int
rune_isspace(Rune r){
	switch(r){
	case ' ':
	case '\t':
	case '\v':
	case '\r':
	case '\n':
		return 1;
	default:
		return 0;
	}
}

struct rune_range { Rune first, last; };
static struct rune_range two_width[] = {
	{ 0x1100,  0x115f},
	{ 0x2329,  0x232a},
	{ 0x2e80,  0x2e99},
	{ 0x2e9b,  0x2ef3},
	{ 0x2f00,  0x2fd5},
	{ 0x2ff0,  0x2ffb},
	{ 0x3000,  0x303e},
	{ 0x3041,  0x3096},
	{ 0x3099,  0x30ff},
	{ 0x3105,  0x312d},
	{ 0x3131,  0x318e},
	{ 0x3190,  0x31ba},
	{ 0x31c0,  0x31e3},
	{ 0x31f0,  0x321e},
	{ 0x3220,  0x3247},
	{ 0x3250,  0x32fe},
	{ 0x3300,  0x4dbf},
	{ 0x4e00,  0xa48c},
	{ 0xa490,  0xa4c6},
	{ 0xa960,  0xa97c},
	{ 0xac00,  0xd7a3},
	{ 0xf900,  0xfaff},
	{ 0xfe10,  0xfe19},
	{ 0xfe30,  0xfe52},
	{ 0xfe54,  0xfe66},
	{ 0xfe68,  0xfe6b},
	{ 0xff01,  0xff60},
	{ 0xffe0,  0xffe6},
	{0x1b000, 0x1b001},
	{0x1f200, 0x1f202},
	{0x1f210, 0x1f23a},
	{0x1f240, 0x1f248},
	{0x1f250, 0x1f251},
	{0x20000, 0x2fffd},
	{0x30000, 0x3fffd},
};

static struct rune_range zero_width[] = {
	{ 0x0300,  0x034e},
	{ 0x0350,  0x036f},
	{ 0x0483,  0x0487},
	{ 0x0591,  0x05bd},
	{ 0x05bf,  0x05bf},
	{ 0x05c1,  0x05c2},
	{ 0x05c4,  0x05c5},
	{ 0x05c7,  0x05c7},
	{ 0x0610,  0x061a},
	{ 0x064b,  0x065f},
	{ 0x0670,  0x0670},
	{ 0x06d6,  0x06dc},
	{ 0x06df,  0x06e4},
	{ 0x06e7,  0x06e8},
	{ 0x06ea,  0x06ed},
	{ 0x0711,  0x0711},
	{ 0x0730,  0x074a},
	{ 0x07eb,  0x07f3},
	{ 0x0816,  0x0819},
	{ 0x081b,  0x0823},
	{ 0x0825,  0x0827},
	{ 0x0829,  0x082d},
	{ 0x0859,  0x085b},
	{ 0x08e4,  0x08ff},
	{ 0x093c,  0x093c},
	{ 0x094d,  0x094d},
	{ 0x0951,  0x0954},
	{ 0x09bc,  0x09bc},
	{ 0x09cd,  0x09cd},
	{ 0x0a3c,  0x0a3c},
	{ 0x0a4d,  0x0a4d},
	{ 0x0abc,  0x0abc},
	{ 0x0acd,  0x0acd},
	{ 0x0b3c,  0x0b3c},
	{ 0x0b4d,  0x0b4d},
	{ 0x0bcd,  0x0bcd},
	{ 0x0c4d,  0x0c4d},
	{ 0x0c55,  0x0c56},
	{ 0x0cbc,  0x0cbc},
	{ 0x0ccd,  0x0ccd},
	{ 0x0d4d,  0x0d4d},
	{ 0x0dca,  0x0dca},
	{ 0x0e38,  0x0e3a},
	{ 0x0e48,  0x0e4b},
	{ 0x0eb8,  0x0eb9},
	{ 0x0ec8,  0x0ecb},
	{ 0x0f18,  0x0f19},
	{ 0x0f35,  0x0f35},
	{ 0x0f37,  0x0f37},
	{ 0x0f39,  0x0f39},
	{ 0x0f71,  0x0f72},
	{ 0x0f74,  0x0f74},
	{ 0x0f7a,  0x0f7d},
	{ 0x0f80,  0x0f80},
	{ 0x0f82,  0x0f84},
	{ 0x0f86,  0x0f87},
	{ 0x0fc6,  0x0fc6},
	{ 0x1037,  0x1037},
	{ 0x1039,  0x103a},
	{ 0x108d,  0x108d},
	{ 0x135d,  0x135f},
	{ 0x1714,  0x1714},
	{ 0x1734,  0x1734},
	{ 0x17d2,  0x17d2},
	{ 0x17dd,  0x17dd},
	{ 0x18a9,  0x18a9},
	{ 0x1939,  0x193b},
	{ 0x1a17,  0x1a18},
	{ 0x1a60,  0x1a60},
	{ 0x1a75,  0x1a7c},
	{ 0x1a7f,  0x1a7f},
	{ 0x1ab0,  0x1abd},
	{ 0x1b34,  0x1b34},
	{ 0x1b44,  0x1b44},
	{ 0x1b6b,  0x1b73},
	{ 0x1baa,  0x1bab},
	{ 0x1be6,  0x1be6},
	{ 0x1bf2,  0x1bf3},
	{ 0x1c37,  0x1c37},
	{ 0x1cd0,  0x1cd2},
	{ 0x1cd4,  0x1ce0},
	{ 0x1ce2,  0x1ce8},
	{ 0x1ced,  0x1ced},
	{ 0x1cf4,  0x1cf4},
	{ 0x1cf8,  0x1cf9},
	{ 0x1dc0,  0x1df5},
	{ 0x1dfc,  0x1dff},
	{ 0x20d0,  0x20dc},
	{ 0x20e1,  0x20e1},
	{ 0x20e5,  0x20f0},
	{ 0x2cef,  0x2cf1},
	{ 0x2d7f,  0x2d7f},
	{ 0x2de0,  0x2dff},
	{ 0x302a,  0x302f},
	{ 0x3099,  0x309a},
	{ 0xa66f,  0xa66f},
	{ 0xa674,  0xa67d},
	{ 0xa69f,  0xa69f},
	{ 0xa6f0,  0xa6f1},
	{ 0xa806,  0xa806},
	{ 0xa8c4,  0xa8c4},
	{ 0xa8e0,  0xa8f1},
	{ 0xa92b,  0xa92d},
	{ 0xa953,  0xa953},
	{ 0xa9b3,  0xa9b3},
	{ 0xa9c0,  0xa9c0},
	{ 0xaab0,  0xaab0},
	{ 0xaab2,  0xaab4},
	{ 0xaab7,  0xaab8},
	{ 0xaabe,  0xaabf},
	{ 0xaac1,  0xaac1},
	{ 0xaaf6,  0xaaf6},
	{ 0xabed,  0xabed},
	{ 0xfb1e,  0xfb1e},
	{ 0xfe20,  0xfe2d},
	{0x101fd, 0x101fd},
	{0x102e0, 0x102e0},
	{0x10376, 0x1037a},
	{0x10a0d, 0x10a0d},
	{0x10a0f, 0x10a0f},
	{0x10a38, 0x10a3a},
	{0x10a3f, 0x10a3f},
	{0x10ae5, 0x10ae6},
	{0x11046, 0x11046},
	{0x1107f, 0x1107f},
	{0x110b9, 0x110ba},
	{0x11100, 0x11102},
	{0x11133, 0x11134},
	{0x11173, 0x11173},
	{0x111c0, 0x111c0},
	{0x11235, 0x11236},
	{0x112e9, 0x112ea},
	{0x1133c, 0x1133c},
	{0x1134d, 0x1134d},
	{0x11366, 0x1136c},
	{0x11370, 0x11374},
	{0x114c2, 0x114c3},
	{0x115bf, 0x115c0},
	{0x1163f, 0x1163f},
	{0x116b6, 0x116b7},
	{0x16af0, 0x16af4},
	{0x16b30, 0x16b36},
	{0x1bc9e, 0x1bc9e},
	{0x1d165, 0x1d169},
	{0x1d16d, 0x1d172},
	{0x1d17b, 0x1d182},
	{0x1d185, 0x1d18b},
	{0x1d1aa, 0x1d1ad},
	{0x1d242, 0x1d244},
	{0x1e8d0, 0x1e8d6},
};

static int
rune_is_in(struct rune_range *table, int start, int end, Rune r){
	int middle = (start + end)/2;
	if(table[middle].first <= r && r <= table[middle].last)
		return 1;
	if(start == end)
		return 0;
	
	if(r > table[middle].last)
		return rune_is_in(table, middle+1, end, r);
	if(r < table[middle].first)
		return rune_is_in(table, start, middle-1, r);
	return 0; /* we shouldn't get here */
}

#define nelem(a) sizeof(a)/sizeof(*a)

int
rune_width(Rune r){
	if(rune_is_in(zero_width, 0, nelem(zero_width)-1, r))
		return 0;
	if(rune_is_in(two_width, 0, nelem(two_width)-1, r))
		return 2;
	return 1;
}
