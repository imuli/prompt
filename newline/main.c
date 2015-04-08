#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "newline.h"

int
write_all(int fd, char *str, int len){
	int i, off = 0;
	while(off < len){
		if((i = write(fd, str+off, len-off)) < 0) return i;
		off += i;
	}
	return off;
}

double lines;
struct termios termp;

static void
raw_mode(struct termios termp){
	cfmakeraw(&termp);
	tcsetattr(0, TCSANOW, &termp);
}

static void
finish(int n){
	newline_cleanup();
	tcsetattr(0, TCSADRAIN, &termp);
	exit(n);
}

int
newline_in(char *str, int len){
	int i;
	if((i = read(0, str, len)) < 0) finish(1);
	return i;
}

int
newline_lineout(char *str, int len){
	if(write_all(1, str, len) < 0) finish(1);
	if(--lines <= 0)
		finish(0);
	return len;
}

int
newline_out(char *str, int len){
	if(write_all(2, str, len) < 0) finish(1);
	return len;
}

int
main(int argc, char** argv){
	lines = strtod(argc>1 ? argv[argc-1] : "inf", NULL);

	if(tcgetattr(0, &termp) == 0)
		raw_mode(termp);

	newline();

	return 1;
}
