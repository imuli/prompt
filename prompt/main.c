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

const int debug = 0;

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

static void
unblock(int fd){
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static void
block(int fd){
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
}

static int
fork_newline(int *infd, int *linefd, int *outfd, int pty){
	int pipes[6];
	int i;
	for(i=0;i<6;i+=2)
		if(pipe(pipes+i) < 0) return 0;
	switch(fork()){
	case -1:
		return 0;
	case 0:
		/* even for reading, odd for writing */
		dup2(pipes[0], 0);
		dup2(pipes[3], 1);
		dup2(pipes[5], 2);
		close(pty);
		for(i=0;i<6;i++)
			close(pipes[i]);
		execlp("bin/newline", "newline", "inf", NULL);
		return 0;
	default:
		/* even for reading, odd for writing */
		close(pipes[0]);
		close(pipes[3]);
		close(pipes[5]);
		*infd = pipes[1];
		*linefd = pipes[2];
		*outfd = pipes[4];
		return 1;
	}
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

enum {
	Stdin = 0,
	Stdout,
	Pty,
	NLin,
	NLline,
	NLout,
	Npoll,
	Bufsz = 1024,
};

int
loop(int pty){
	int i, stop = 0;
	int edit = 0;
	int nlin, nlout, nlline;

	struct termios termp = term_orig;
	cfmakeraw(&termp);
	termp.c_lflag &= ~ECHO;
	if(tcsetattr(0, TCSANOW, &termp)!=0) error("tcsetattr");

	Buffer in, out, nl;
	in = buffer_alloc();
	out = buffer_alloc();
	nl = buffer_alloc();
	if(in == NULL || out == NULL || nl == NULL)
		error("buffer_alloc");

	if(!fork_newline(&nlin, &nlline, &nlout, pty)) error("fork newline");

	struct pollfd pfd[Npoll];
	pfd[Stdin].fd = 0;
	pfd[Stdout].fd = 1;
	pfd[Pty].fd = pty;
	pfd[NLin].fd = nlin;
	pfd[NLline].fd = nlline;
	pfd[NLout].fd = nlout;

	unblock(0);
	unblock(1);
	unblock(pty);
	unblock(nlin);
	unblock(nlline);
	unblock(nlout);

	/* raw mode:
	 * stdin  → ptm
	 * stdout ← ptm
	 *
	 * canonical mode:
	 * stdin → nlin
	 * stdout ← nlout
	 * stdout ← ptm
	 * nlline → ptm 
	 */

	while(!stop){
		for(i=0;i<Npoll;i++){
			pfd[i].events = 0;
		}

		if(buffer_space(nl))	pfd[NLline].events |= POLLIN;
		if(buffer_space(in))	pfd[Stdin].events |= POLLIN;
		if(buffer_space(out))	pfd[NLout].events |= POLLIN;
		if(buffer_space(out))	pfd[Pty].events |= POLLIN;

		if(buffer_amount(nl))	pfd[Pty].events |= POLLOUT;
		if(buffer_amount(in))	pfd[Pty].events |= POLLOUT;
		if(buffer_amount(in))	pfd[NLin].events |= POLLOUT;
		if(buffer_amount(out))	pfd[Stdout].events |= POLLOUT;

		poll(pfd, Npoll, -1);

		update_termios(pty, &termp);
		edit = termp.c_lflag & ICANON;

		if(pfd[Pty].revents & POLLIN)
			buffer_pull(out, pty);
		if(pfd[NLout].revents & POLLIN)
			buffer_pull(out, nlout);
		if(pfd[Stdin].revents & POLLIN)
			buffer_pull(in, 0);
		if(pfd[NLline].revents & POLLIN)
			buffer_pull(nl, nlline);

		if(pfd[Stdout].revents & POLLOUT)
			buffer_push(1, out);
		if(pfd[Pty].revents & POLLOUT)
			buffer_push(pty, nl);
		if(pfd[NLin].revents & POLLOUT && edit)
			buffer_push(nlin, in);
		if(pfd[Pty].revents & POLLOUT && !edit)
			buffer_push(pty, in);

		stop |= pfd[Stdin].revents & POLLHUP;
		stop |= pfd[Stdout].revents & POLLHUP;
		stop |= pfd[Pty].revents & POLLHUP;
		stop |= pfd[NLin].revents & POLLERR;
		stop |= pfd[NLline].revents & POLLERR;
		stop |= pfd[NLout].revents & POLLERR;
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
