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
	e->end = e->pos = e->line;
	return e;
}

enum {
	EOT	= 0x04,
	LF	= 0x0a,
	CR	= 0x0d,
	CTL	= 0x1f,
};

static int
char_width(unsigned char c){
		/* printable characters */
	if(	(c >= L' ' && c < L'\x80') ||
		/* utf8, FIXME combining characters */
		(c >= L'\xc2' && c < L'\xfe')) return 1;
	return 0;
}

static int
terminal_width(const char *s, const char *e){
	int t = 0;
	for(; s<e; s++){
		t += char_width(*s);
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
	char *here;
	switch(c){
	case CTL&'a':	/* beginning of line */
		cursor_shift(e, -terminal_width(e->line, e->pos));
		e->pos = e->line;
		break;
	case CTL&'b':	/* back one */
		if(e->pos == e->line) break;
		while(--e->pos > e->line && char_width(*e->pos) == 0);
		cursor_shift(e, -1);
		break;
	case CTL&'c':	/* break */
		e->pos = e->end = e->line;
		act = Append|Flush;
		break;
	case CTL&'d':	/* end text */
		act = Append|Flush;
		break;
	case CTL&'e':	/* end of line */
		cursor_shift(e, terminal_width(e->pos, e->end));
		e->pos = e->end;
		break;
	case CTL&'f':	/* forward one */
		if(e->pos == e->end) break;
		while(++e->pos < e->end && char_width(*e->pos) == 0);
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
		*e->end++ = c;
	}
	if(act & Copy){
		memmove(e->pos + 1, e->pos, e->end - e->pos);
		*e->pos++ = c;
		e->end++;
	}
	if(act & Echo && e->echo){
		cursor_shift(e, -terminal_width(e->line, e->pos));
		erase_line(e);
		buffer_add(e->term, e->line, e->end - e->line);
		cursor_shift(e, -terminal_width(e->pos, e->end));
	}
	if(act & Flush){
		cursor_shift(e, -terminal_width(e->line, e->pos));
		buffer_add(e->pty, e->line, e->end - e->line);
		e->pos = e->end = e->line;
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
