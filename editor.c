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
redraw_line(Editor e, char c){
	if(!e->echo) return;
	cursor_shift(e, -terminal_width(e->line, e->pos));
	erase_line(e);
	buffer_add(e->term, e->line, e->end - e->line);
	cursor_shift(e, -terminal_width(e->pos, e->end));
}

static void
append_character(Editor e, char c){
	*e->end++ = c;
}

static void
insert_character(Editor e, char c){
	memmove(e->pos + 1, e->pos, e->end - e->pos);
	*e->pos++ = c;
	e->end++;
	redraw_line(e, c);
}

static void
flush_line(Editor e, char c){
	cursor_shift(e, -terminal_width(e->line, e->pos));
	buffer_add(e->pty, e->line, e->end - e->line);
	e->pos = e->end = e->line;
}

static void
start_of_line(Editor e, char c){
	cursor_shift(e, -terminal_width(e->line, e->pos));
	e->pos = e->line;
}

static void
backward_char(Editor e, char c){
	if(e->pos == e->line) return;
	while(--e->pos > e->line && char_width(*e->pos) == 0);
	cursor_shift(e, -1);
}

static void
interrupt(Editor e, char c){
	e->pos = e->end = e->line;
	append_character(e, CTL&'c');
	flush_line(e, c);
}

static void
end_of_file(Editor e, char c){
	append_character(e, CTL&'d');
	flush_line(e, c);
}

static void
end_of_line(Editor e, char c){
	cursor_shift(e, terminal_width(e->pos, e->end));
	e->pos = e->end;
}

static void
forward_char(Editor e, char c){
	if(e->pos == e->end) return;
	while(++e->pos < e->end && char_width(*e->pos) == 0);
	cursor_shift(e, 1);
}

static void
backspace_char(Editor e, char c){
	char *here;
	if(e->pos == e->line) return;
	here = e->pos;
	while(--e->pos > e->line && char_width(*e->pos) == 0);
	cursor_shift(e, -2);
	memmove(e->pos, here, e->end - here);
	e->end -= here - e->pos;
	redraw_line(e, c);
}

static void
kill_to_end(Editor e, char c){
	e->end = e->pos;
	redraw_line(e, c);
}

static void
send_line(Editor e, char c){
	append_character(e, '\r');
	flush_line(e, c);
}

static void
kill_to_start(Editor e, char c){
	cursor_shift(e, -terminal_width(e->line, e->pos));
	memmove(e->line, e->pos, e->end - e->pos);
	e->end -= e->pos - e->line;
	e->pos = e->line;
	redraw_line(e, c);
}

struct keyconfig {
	char c;
	void (*func)(Editor, char);
};

static struct keyconfig
keys_normal[] = {
	{CTL&'a', start_of_line},
	{CTL&'b', backward_char},
	{CTL&'c', interrupt},
	{CTL&'d', end_of_file},
	{CTL&'e', end_of_line},
	{CTL&'f', forward_char},
	{CTL&'h', backspace_char},
	{CTL&'k', kill_to_end},
	{CTL&'l', redraw_line},
	{CTL&'m', send_line},
	{CTL&'u', kill_to_start},
	{0x7f, backspace_char},
	{0, insert_character},
};

static void
editor_char(Editor e, char c){
	struct keyconfig *k;
	for(k = keys_normal; k->c!=0; k++)
		if(k->c == c) break;
	k->func(e, c);
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
