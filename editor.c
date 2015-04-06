#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"
#include "rune.h"
#include "text.h"
#include "editor.h"

enum {
	Linesize = 128,
};

void *
editor_free(Editor e){
	if(e == NULL) return NULL;
	if(e->line) text_free(e->line);
	free(e);
	return NULL;
}

Editor
editor_init(Buffer pty, Buffer term){
	Editor e;
	if((e = calloc(1, sizeof(*e))) == NULL) return editor_free(e);
	e->pty = pty;
	e->term = term;
	if((e->line = text_new(Linesize)) == NULL) return editor_free(e);
	if((e->yank = text_new(Linesize)) == NULL) return editor_free(e);
	return e;
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

static int
word_len(Editor e, int dir){
	Rune *r = e->line->buf;
	int i = e->line->off;
	if(dir < 0) i--;
	for(; i>=0 && i<e->line->len && rune_isspace(r[i]); i+=dir);
	for(; i>=0 && i<e->line->len && !rune_isspace(r[i]); i+=dir);
	if(dir < 0) i++;
	return i - e->line->off;
}

static void
kill(Editor e, int len){
	if(!e->kill_roll) text_clear(e->yank);
	e->kill_roll=2;
	if(len < 0){
		cursor_shift(e, -e->line->off);
		text_shift(e->line, len);
		len = -len;
		text_shift(e->yank, -e->yank->off);
	} else {
		text_shift(e->yank, e->yank->len - e->yank->off);
	}
	text_insert(e->yank, e->line->buf + e->line->off, len);
	text_delete(e->line, len);
}

enum {
	CTL	= 0x1f,
};

static void
redraw_line(Editor e, Rune c){
	if(!e->echo) return;
	cursor_shift(e, -e->line->off);
	erase_line(e);
	text_render(e->line);
	buffer_add(e->term, e->line->text, e->line->textlen);
	cursor_shift(e, e->line->off - e->line->len);
}

static void
append_character(Editor e, Rune c){
	text_append(e->line, &c, 1);
}

static void
insert_character(Editor e, Rune c){
	text_insert(e->line, &c, 1);
	redraw_line(e, c);
}

static void
flush_line(Editor e, Rune c){
	cursor_shift(e, -e->line->off);
	text_render(e->line);
	buffer_add(e->pty, e->line->text, e->line->textlen);
	text_clear(e->line);
}

static void
start_of_line(Editor e, Rune c){
	int n = -e->line->off;
	text_shift(e->line, n);
	cursor_shift(e, n);
}

static void
backward_char(Editor e, Rune c){
	int n = text_shift(e->line, -1);
	cursor_shift(e, n);
}

static void
interrupt(Editor e, Rune c){
	text_clear(e->line);
	append_character(e, c);
	flush_line(e, c);
}

static void
end_of_file(Editor e, Rune c){
	append_character(e, CTL&'d');
	flush_line(e, c);
}

static void
end_of_line(Editor e, Rune c){
	int n = e->line->len - e->line->off;
	cursor_shift(e, n);
	text_shift(e->line, n);
}

static void
forward_char(Editor e, Rune c){
	int n = text_shift(e->line, 1);
	cursor_shift(e, n);
}

static void
backspace_char(Editor e, Rune c){
	cursor_shift(e, -1);
	text_delete(e->line, -1);
	redraw_line(e, c);
}

static void
kill_to_end(Editor e, Rune c){
	kill(e, e->line->len - e->line->off);
	redraw_line(e, c);
}

static void
send_line(Editor e, Rune c){
	append_character(e, '\r');
	flush_line(e, c);
}

static void
kill_to_start(Editor e, Rune c){
	kill(e, -e->line->off);
	redraw_line(e, c);
}

static void
kill_word(Editor e, Rune c){
	kill(e, word_len(e, -1));
	redraw_line(e, c);
}

static void
yank(Editor e, Rune c){
	text_insert(e->line, e->yank->buf, e->yank->len);
	cursor_shift(e, e->yank->len);
	redraw_line(e, c);
}

struct keyconfig {
	Rune r;
	void (*func)(Editor, Rune);
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
	{CTL&'y', yank},
	{CTL&'w', kill_word},
	{0x7f, backspace_char},
	{0, insert_character},
};

static void
editor_rune(Editor e, Rune r){
	struct keyconfig *k;
	for(k = keys_normal; k->r!=0; k++)
		if(k->r == r) break;
	k->func(e, r);
	e->kill_roll >>= 1;
}

void
editor(Editor e, int fd){
	int len, ul;
	char buf[256], *b;
	Rune r;
	len = read(fd, buf, sizeof(buf));
	if(len <= 0) return;

	for(b=buf;len > 0; len-=ul){
		ul = rune_utf8(&r, b);
		if(ul == 0) break; /* FIXME this discards partial runes */
		editor_rune(e, r);
	}
}
