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
	e->off = 0;
	return e;
}

enum {
	EOT	= 0x04,
	LF	= 0x0a,
	CR	= 0x0d,
	ESC	= 0x1b,
	CTL	= 0x1f,
};

static int
char_width(char c){
		/* printable characters */
	if(	(c >= ' ' && c < '\x7f') ||
		/* utf8, FIXME combining characters */
		(c >= '\xc2' && c < '\xfe')) return 1;
	return 0;
}

static int
terminal_width(char *s, int n){
	int t = 0;
	for(; n>0; n--){
		t += char_width(*s++);
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

static void
editor_char(Editor e, char c){
	enum { Append = 1, Copy = 2, Echo = 4, Flush = 8 } act = 0;
	switch(c){
	case CTL&'a':	/* beginning of line */
		cursor_shift(e, -terminal_width(e->line, e->off));
		e->off = 0;
		break;
	case CTL&'b':	/* back one */
		if(e->off == 0) break;
		while(--e->off > 0 && char_width(e->line[e->off]) == 0);
		cursor_shift(e, -1);
		break;
	case CTL&'c':	/* break */
		e->len = e->off = 0;
		act = Append|Flush;
		break;
	case CTL&'d':	/* end text */
		act = Append|Flush;
		break;
	case CTL&'e':	/* end of line */
		cursor_shift(e, terminal_width(e->line + e->off, e->len - e->off));
		e->off = e->len;
		break;
	case CTL&'f':	/* forward one */
		if(e->off == e->len) break;
		while(++e->off < e->len && char_width(e->line[e->off]) == 0);
		cursor_shift(e, 1);
		break;
	case CTL&'g':	/* bell? */
		break;
	case LF:
	case CR:
		act = Append|Flush;
		break;
	default:
		act = Copy|Echo;
		break;
	}

	if(act & Append){
		e->line[e->len++] = c;
	}
	if(act & Copy){
		memmove(e->line + e->off + 1, e->line + e->off, e->len - e->off);
		e->line[e->off++] = c;
		e->len++;
	}
	if(act & Echo && e->echo){
		buffer_add(e->term, e->line + e->off - 1, e->len - e->off + 1);
		cursor_shift(e, -terminal_width(e->line + e->off, e->len - e->off));
	}
	if(act & Flush){
		cursor_shift(e, -terminal_width(e->line, e->off));
		buffer_add(e->pty, e->line, e->len);
		e->len = e->off = 0;
	}
}

void
editor(Editor e, int fd){
	int len;
	char buf[256], *b;
	len = read(fd, buf, sizeof(buf));
	if(len <= 0) return;

	for(b=buf;len > 0; len--)
		editor_char(e, *b++);
}
