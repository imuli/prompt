#ifndef RUNE_H
#define RUNE_H
#include <limits.h>
#if LONG_BIT == 32
typedef unsigned long Rune;
#else
typedef unsigned int Rune;
#endif
#define RUNE_MAX 0xffffffff
#define UTF8_MAX 7
enum {
	BadRune = 0xffff,
};

int runechar(Rune r);
int utf8char(char *u);
int rune_utf8(Rune *rr, char *u);
int utf8_rune(char *u, Rune r);

int runeschars(Rune *r);
int utf8srunes(char *u);
int runes_utf8s(Rune *r, char *u);
int utf8s_runes(char *u, Rune *r);

#endif
