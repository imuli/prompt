#include <rune.h>
int utf8_rune(char *u, Rune r){
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
