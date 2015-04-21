#include <rune.h>
int runeschars(Runes* r){
	int n = 0, i;
	for(i=r->c;i>0;i--){
		n += runechar(r->r[i]);
	}
	return n;
}
