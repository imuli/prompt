#include <rune.h>
int rune_isspace(Rune r){
	switch(r){
	case ' ':
	case '\t':
	case '\v':
	case '\r':
	case '\n':
		return 1;
	default:
		return 0;
	}
}
