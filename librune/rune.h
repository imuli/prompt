#ifndef RUNE_H
#define RUNE_H
#include <limits.h>
#if LONG_BIT == 32
typedef unsigned long Rune;
#else
typedef unsigned int Rune;
#endif
typedef struct {
	Rune c;
	Rune r[];
} Runes;
#define RUNE_MAX 0xffffffff
#define UTF8_MAX 7
enum {
	IncRune = 0xfffe,
	BadRune = 0xffff,
};

int runechar(Rune r);
int utf8char(char *u);
int rune_utf8(Rune *rr, char *u);
int utf8_rune(char *u, Rune r);

int runeschars(Runes *r);
int utf8srunes(char *u);
int runes_utf8s(Runes *r, char *u);
int utf8s_runes(char *u, Runes *r);

int rune_isspace(Rune r);
int rune_width(Rune r);

int log2rune(Rune r);

#endif
