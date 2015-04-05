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
erase_line(Editor e){
	char cmd[]="\e[K";
	buffer_add(e->term, cmd, sizeof(cmd)-1);
}

enum {
	CTL	= 0x1f,
};

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
	case 0x7f:
	case CTL&'h':	/* backspace */
		if(e->pos == e->line) break;
		here = e->pos;
		while(--e->pos > e->line && char_width(*e->pos) == 0);
		cursor_shift(e, -2);
		memmove(e->pos, here, e->end - here);
		e->end -= here - e->pos;
		act = Echo;
		break;
	case CTL&'i':	/* tab */
		break;
	case CTL&'k':	/* kill to end */
		e->end = e->pos;
		act = Echo;
		break;
	case CTL&'j':	/* Line Feed */
		break;
	case CTL&'l':	/* redraw */
		act = Echo;
		break;
	case CTL&'m':	/* Return */
		act = Append|Flush;
		break;
	case CTL&'n':	/* next in history */
		break;
	case CTL&'o':	/* */
		break;
	case CTL&'p':	/* previous in history */
		break;
	case CTL&'q':	/* */
		break;
	case CTL&'r':	/* reverse search */
		break;
	case CTL&'s':	/* forward search? */
		break;
	case CTL&'t':	/* transpose two characters */
		break;
	case CTL&'u':	/* kill to start */
		cursor_shift(e, -terminal_width(e->line, e->pos));
		memmove(e->line, e->pos, e->end - e->pos);
		e->end -= e->pos - e->line;
		e->pos = e->line;
		act = Echo;
		break;
	case CTL&'v':	/* literal character comes next */
		break;
	case CTL&'w':	/* kill last word */
		break;
	case CTL&'x':	/* */
		break;
	case CTL&'y':	/* paste kill buffer */
		break;
	case CTL&'z':	/* */
		break;
	case CTL&'[':	/* Escape Sequence */
		break;
	case CTL&'|':	/* */
		break;
	case CTL&']':	/* */
		break;
	case CTL&'^':	/* */
		break;
	case CTL&'_':	/* */
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
