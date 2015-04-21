#include <rune.h>
int utf8srunes(char *u){
	int n, i = 0;
	for(;*u!=0;u+=n){
		n = utf8char(u);
		if(n < 1) return -1;
		i++;
	}
	return i;
}
