#include <rune.h>
int utf8s_runes(char *u, Runes* r){
	int n = 0, i;
	for(i=r->c;i>0;i--){
		n += utf8_rune(u+n, r->r[i]);
	}
	*u = '\0';
	return n;
}
