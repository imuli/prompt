#include <errno.h>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

char *argv0;
struct termios term_orig;
int die = 0;

void
sig_chld(int) {
	die = 1;
}

int
error(char *msg){
	fprintf(stderr, "%s: %s: %s\n", argv0, msg, strerror(errno));
	exit(1);
	return 1;
}

enum {
	BufSize = 1024,
};

int
writeall(int to, char* buf, int sz){
	int len, off;
	for(off=0; off < sz; off+=len){
		len = write(to, buf+off, sz-off);
		if(len < 0) return -off;
	}
	return sz;
}

static int
inputproc(int pty){
	char buf[BufSize];
	int len;
	struct winsize winp;
	struct termios termp;

	termp = term_orig;
	cfmakeraw(&termp);
	termp.c_lflag &= ~ECHO;
	if(tcsetattr(0, TCSANOW, &termp)!=0) error("tcsetattr");

	while(1){
		len = read(0, buf, BufSize);
		if(len > 0){
			if(writeall(pty, buf, len) <= 0) break;
		} else if(len < 0 && errno == EINTR){
			ioctl(0, TIOCGWINSZ, (char *)&winp);
			ioctl(pty, TIOCSWINSZ, (char *)&winp);
		} else {
			break;
		}
	}
	if(tcsetattr(0, TCSADRAIN, &term_orig)!=0) error("tcsetattr");
	return 0;
}

int
outputproc(int pty){
	char buf[BufSize];
	int len;
	while(1){
		len = read(pty, buf, BufSize);
		if(len > 0){
			if(writeall(0, buf, len) <= 0) break;
		} else {
			break;
		}
	}
	return 0;
}

int
child(int pty, int argc, char** argv){
	if(login_tty(pty) < 0)
		return error("login_tty");
	execv("/bin/bash", argv+1);
	return error("exec");
}

static void
initpty(int* ptm, int* pts){
	struct winsize winp;

	if(tcgetattr(0, &term_orig)!=0) error("tcgetattr");
	ioctl(0, TIOCGWINSZ, (char *)&winp);

	if(openpty(ptm, pts, NULL, &term_orig, &winp) < 0)
		error("openpty");
}

int
main(int argc, char** argv){
	int ptm, pts;
	argv0 = *argv;

	initpty(&ptm, &pts);

	switch(fork()){
	case -1: return error("forkpty");
	case 0: break;
	default:
		close(pts);
		return inputproc(ptm);
	}

	switch(fork()){
	case -1: return error("fork");
	case 0: break;
	default:
		close(pts);
		return outputproc(ptm);
	}
	close(ptm);
	return child(pts, argc, argv);
}
