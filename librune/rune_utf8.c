#include <rune.h>
int rune_utf8(Rune *rr, char *u){
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
