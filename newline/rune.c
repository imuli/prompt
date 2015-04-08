#include <string.h>
#include <stdio.h>
#include "rune.h"

static const unsigned int biti_32v[32] = {
   0, 1,28, 2,29,14,24, 3,	30,22,20,15,25,17, 4, 8,
  31,27,13,23,21,19,16, 7,	26,12,18, 6,11, 5,10, 9};
static const unsigned long biti_32m = 0x077cb531;
static int
bit_index(unsigned r) {
  unsigned i = r;
  i |= i >> 1; i |= i >> 2; i |= i >> 4;
  i |= i >> 8; i |= i >> 16;
  i=i/2+1;
  return biti_32v[((i * biti_32m) >> 27)%32];
}

int
runechar(Rune r){
	int i = bit_index(r);
	if(i<7) return 1;
	return (i-1)/5+1;
}

int
runeschars(Rune *r){
	int i = 0;
	for(;*r!=0;r++){
		i+= runechar(*r);
	}
	return i;
}

int
utf8char(char *u){
	int i = bit_index(0xff&~*u);
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
runes_utf8s(Rune *r, char *u){
	int n,i = 0;
	for(;*u!=0;u+=n){
		n = rune_utf8(r, u);
		r++;
		i++;
	}
	*r = 0;
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
utf8s_runes(char *u, Rune *r){
	int n,i = 0;
	for(;*r!=0;u+=n){
		n = utf8_rune(u, *r);
		r++;
		i+=n;
	}
	*u = '\0';
	return i;
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

