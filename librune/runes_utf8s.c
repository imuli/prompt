#include <rune.h>
int runes_utf8s(Runes* r, char *u){
	int n = 0, i = r->c;
	for(;u[n]!=0;i++){
		n += rune_utf8(r->r+i, u+n);
	}
	r->c = i;
	return i;
}
