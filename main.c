#include <errno.h>
#include <poll.h>
#include <pty.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

char *argv0;

void
error(char *msg){
	fprintf(stderr, "%s: %s: %s\n", argv0, msg, strerror(errno));
	exit(1);
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

int
fdcat(int to, int from){
	char buf[BufSize];
	int total, sz, len;
	total = 0;
	while((sz = read(from, buf, BufSize)) > 0){
		if((len = writeall(to, buf, sz)) < 0)
			return len;
		total += sz;
	}
	return total;
}

static void*
inputthread(void* arg){
	int* fd = arg;
	char buf[BufSize];
	int len;
	struct winsize winp;
	while(1){
		len = read(fd[1], buf, BufSize);
		if(len > 0){
			if(writeall(fd[0], buf, len) <= 0) break;
		} else if(len < 0 && errno == EINTR){
			ioctl(fd[1], TIOCGWINSZ, (char *)&winp);
			ioctl(fd[0], TIOCSWINSZ, (char *)&winp);
		} else {
			break;
		}
	}
	return NULL;
}

void
master(int pty, int pid){
	pthread_t thread[2];
	pthread_attr_t attr;
	int fds[2];

	if((errno = pthread_attr_init(&attr)) != 0)
		error("pthread_attr_init");
	if((errno = pthread_attr_setstacksize(&attr, 16*1024)) != 0)
		error("pthread_attr_setstacksize");

	fds[0] = pty; fds[1] = 0;
	if((errno = pthread_create(&thread[0], &attr, inputthread, fds)) != 0)
		error("pthread_create");
	fdcat(1, pty);
}

void
child(int argc, char** argv){
	execv("/bin/bash", argv+1);
}

int
main(int argc, char** argv){
	pid_t pid;
	int pty;
	struct termios termp;
	argv0 = *argv;

	if(tcgetattr(0, &termp)!=0) error("tcgetattr");

	pid = forkpty(&pty, NULL, &termp, NULL/*winp*/);
	switch(pid){
	case -1:
		error("forkpty");
		break;
	case 0:
		child(argc, argv);
		break;
	default:
		master(pty, pid);
		break;
	}
	return 0;
}
