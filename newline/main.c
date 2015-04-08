#include <signal.h>
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

double lines = 1;
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

static void
handlesignals(int sig){
	if(sig == SIGPIPE) finish(0);
	finish(1);
}

static void
setsignals(void){
	struct sigaction sa = { { handlesignals } };
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
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
	if(argc>1)
		lines = strtod(argv[argc-1], NULL);
	setsignals();

	if(tcgetattr(0, &termp) == 0)
		raw_mode(termp);

	newline();

	return 1;
}
