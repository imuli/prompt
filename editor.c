#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"
#include "editor.h"

enum {
	Linesize = 1024,
};

void *
editor_free(Editor e){
	if(e == NULL) return NULL;
	if(e->line) free(e->line);
	free(e);
	return NULL;
}

Editor
editor_init(Buffer pty, Buffer term){
	Editor e;
	if((e = calloc(1, sizeof(*e))) == NULL) return editor_free(e);
	e->pty = pty;
	e->term = term;
	if((e->line = malloc(Linesize)) == NULL) return editor_free(e);
	e->offset = 0;
	return e;
}

enum {
	EOT	= 0x04,
	LF	= 0x0a,
	CR	= 0x0d,
	ESC	= 0x1b,
};

int
terminal_width(char *s, int n){
	int t = 0;
	for(; n>0; n--){
		/* printable characters */
		if(*s >= ' ' && *s < '\x7f') t++;
		/* utf8, disregarding combining characters */
		if(*s >= '\xc2' && *s < '\xfe') t++;
		s++;
	}
	return t;
}

static void
cursor_shift(Editor e, int n){
	char cmd[12];
	int len;
	if(n==0) return;
	len = snprintf(cmd, 12, "\e[%d%c", n, n<0? 'D' : 'C');
	buffer_add(e->term, cmd, len);
}

void
editor_char(Editor e, char c){
	enum { Copy = 2, Echo = 4, Flush = 8 } act = 0;
	switch(c){
	case EOT:
		act = Flush|Copy;
		break;
	case LF:
	case CR:
		act = Flush|Copy;
		break;
	default:
		act = Copy|Echo;
		break;
	}

	if(act & Copy)
		e->line[e->offset++] = c;
	if(act & Echo && e->echo)
		buffer_add(e->term, &c, 1);
	if(act & Flush){
		cursor_shift(e, -terminal_width(e->line, e->offset));
		buffer_add(e->pty, e->line, e->offset);
		e->offset = 0;
	}
}

void
editor(Editor e, int fd){
	int len;
	char buf[256], *b;
	len = read(fd, buf, sizeof(buf));
	if(len <= 0) return;

	for(b=buf;len > 0; len--)
		editor_char(e, *b);
}
