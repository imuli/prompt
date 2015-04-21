#include <rune.h>
int runechar(Rune r){
	int i = log2rune(r);
	if(i<7) return 1;
	return (i-1)/5+1;
}
