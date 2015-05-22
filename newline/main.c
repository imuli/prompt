#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "newline.h"

struct termios termp;

static void
raw_mode(struct termios termp){
	cfmakeraw(&termp);
	termp.c_oflag |= OPOST;
	tcsetattr(0, TCSANOW, &termp);
}

static void
reset_term(void){
	tcsetattr(0, TCSADRAIN, &termp);
}

static void
handlesignals(int sig){
	if(sig == SIGPIPE) exit(0);
	exit(1);
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

void
set_history_file(void){
	char file[] = "/.newline_history";
	char *home = getenv("HOME");
	history_file = malloc(strlen(home) + sizeof(file));
	sprintf(history_file, "%s%s", home, file);
}

int
main(int argc, char** argv){
	int i;
	for(i=1;i<argc;i++){
		if(argv[i][0] == '-'){
			if(strcmp(argv[i], "--pty") == 0)	pty_mode = 1;
			if(strcmp(argv[i], "--history") == 0)	history_file = argv[++i];
		} else {
			lines = strtod(argv[i], NULL);
		}
	}
	if(lines <= 0) return 1;
	if(history_file == NULL) set_history_file();
	setsignals();

	if(tcgetattr(0, &termp) == 0){
		raw_mode(termp);
		atexit(&reset_term);
	}

	return newline();
}
