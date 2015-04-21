#include <rune.h>
int utf8char(char *u){
	int i = log2rune(0xff&~*u);
	if(i>5) return i-6;
	return 7-i;
}
