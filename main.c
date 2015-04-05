#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

#include "buffer.h"
#include "termios.h"

const int debug = 1;

char *argv0;
struct termios term_orig;
int die = 0;

void
sig_chld(int x) {
	die = 1;
}

int
error(char *msg){
	fprintf(stderr, "%s: %s: %s\n", argv0, msg, strerror(errno));
	exit(1);
	return 1;
}

int
writeall(int to, char* buf, int sz){
	int len, off;
	for(off=0; off < sz; off+=len){
		len = write(to, buf+off, sz-off);
		if(len < 0) return -off;
	}
	return sz;
}

enum {
	Stdin = 0,
	Stdout,
	Pty,
	Npoll,
	Bufsz = 1024,
};

static void
unblock(int fd){
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static void
block(int fd){
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
}

static int
update_termios(int pty, struct termios* termp){
	struct termios termq;
	if(tcgetattr(pty, &termq)!=0) return -1;
	if(memcmp(termp, &termq, sizeof(termq)) != 0){
		if(debug) compare_termios(termp, &termq);
		memcpy(termp, &termq, sizeof(termq));
		return 1;
	}
	return 0;
}

int
loop(int pty){
	int i, stop = 0;

	struct termios termp = term_orig;
	cfmakeraw(&termp);
	termp.c_lflag &= ~ECHO;
	if(tcsetattr(0, TCSANOW, &termp)!=0) error("tcsetattr");

	Buffer in, out;
	in = buffer_alloc();
	out = buffer_alloc();
	if(in == NULL || out == NULL)
		error("buffer_alloc");

	struct pollfd pfd[Npoll];
	pfd[Stdin].fd = 0;
	pfd[Stdout].fd = 1;
	pfd[Pty].fd = pty;

	unblock(0);
	unblock(1);
	unblock(pty);

	while(!stop){
		for(i=0;i<Npoll;i++){
			pfd[i].events = 0;
		}
		if(buffer_space(in))	pfd[Stdin].events |= POLLIN;
		if(buffer_amount(in))	pfd[Pty].events |= POLLOUT;
		if(buffer_space(out))	pfd[Pty].events |= POLLIN;
		if(buffer_amount(out))	pfd[Stdout].events |= POLLOUT;

		poll(pfd, Npoll, -1);

		if(pfd[Stdin].revents & POLLIN){
			buffer_pull(in, 0);
		}
		if(pfd[Pty].revents & POLLOUT){
			buffer_push(pty, in);
		}

		if(pfd[Pty].revents & POLLIN){
			buffer_pull(out, pty);
			update_termios(pty, &termp);
		}
		if(pfd[Stdout].revents & POLLOUT){
			buffer_push(1, out);
		}

		stop |= pfd[Stdin].revents & POLLHUP;
		stop |= pfd[Stdout].revents & POLLHUP;
		stop |= pfd[Pty].revents & POLLHUP;
	}

	/* flush output buffer */
	block(1);
	while(buffer_amount(out)){
		if(buffer_push(1, out) <= 0) break;
	}

	if(tcsetattr(0, TCSADRAIN, &term_orig)!=0) error("tcsetattr");

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
	case 0:
		close(ptm);
		return child(pts, argc, argv);
	default:
		close(pts);
		return loop(ptm);
	}
}
