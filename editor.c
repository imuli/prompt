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

static void
cursor_shift(Editor e, int n){
	char cmd[12];
	int len;
	if(n==0) return;
	len = snprintf(cmd, 12, "\e[%d%c", n, n<0? 'D' : 'C');
	buffer_add(e->term, cmd, len);
	e->cursor += n;
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
		text_shift(e->line, len);
		len = -len;
		text_shift(e->yank, -e->yank->off);
	} else {
		text_shift(e->yank, e->yank->len - e->yank->off);
	}
	text_insert(e->yank, e->line->buf + e->line->off, len);
	text_delete(e->line, len);
}

static void
shift(Editor e, int n){
	cursor_shift(e, text_shift(e->line, n));
}

static void
redraw_line(Editor e, Rune c){
	if(!e->echo)
		return cursor_shift(e, e->line->off-e->cursor);
	cursor_shift(e, -e->cursor);
	text_render(e->line);
	buffer_add(e->term, e->line->text, e->line->textlen);
	e->cursor += e->line->len;
	erase_line(e);
	cursor_shift(e, e->line->off - e->line->len);
}

static void
append_character(Editor e, Rune c){
	text_append(e->line, &c, 1);
}

static void
insert_character(Editor e, Rune c){
	if(0xd800 <= c && c < 0xdc00) return;
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
backward_line(Editor e, Rune c){
	shift(e, -e->line->off);
}

static void
backward_word(Editor e, Rune c){
	shift(e, word_len(e, -1));
}

static void
backward_char(Editor e, Rune c){
	shift(e, -1);
}

static void
forward_word(Editor e, Rune c){
	shift(e, word_len(e, 1));
}

static void
forward_line(Editor e, Rune c){
	shift(e, e->line->len - e->line->off);
}

static void
forward_char(Editor e, Rune c){
	shift(e, 1);
}

static void
interrupt(Editor e, Rune c){
	text_clear(e->line);
	append_character(e, c);
	flush_line(e, c);
}

static void
end_of_file(Editor e, Rune c){
	append_character(e, 4);
	flush_line(e, c);
}

static void
backspace_char(Editor e, Rune c){
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
kill_backward_word(Editor e, Rune c){
	kill(e, word_len(e, -1));
	redraw_line(e, c);
}

static void
kill_backward_char(Editor e, Rune c){
	kill(e, -1);
	redraw_line(e, c);
}

static void
yank(Editor e, Rune c){
	text_insert(e->line, e->yank->buf, e->yank->len);
	redraw_line(e, c);
}

static void
noop(Editor e, Rune c){
}

struct keyconfig {
	Rune r;
	void (*func)(Editor, Rune);
};

enum {
	Normal	= 0x0,
	Control	= 0x1,
	Meta	= 0x2,
	Shift	= 0x4,

	/* special keys present in unicode */
	Escape	= 0x1b,
	Bksp	= 0x7f,
	/* surrogates should not appear in valid unicode streams,
	 * so are safe to use for function key definitions.
	 */
	Base	= 0xd7ff,
	NoOp,
	Up,
	Down,
	Left,
	Right,
	Prior,
	Next,
	Home,
	End,
	Find,
	Sel,
	Ins,
	Del,
	FKey,
};

static struct keyconfig
keys_normal[] = {
	{0x7f, backspace_char},
	{Right,	forward_char},
	{Left,	backward_char},
	{Home,	backward_line},
	{End,	forward_line},
	{0, insert_character},
};
static struct keyconfig
keys_control[] = {
	{'a', backward_line},
	{'b', backward_char},
	{'c', interrupt},
	{'d', end_of_file},
	{'e', forward_line},
	{'f', forward_char},
	{'h', kill_backward_char},
	{'k', kill_to_end},
	{'l', redraw_line},
	{'m', send_line},	/* Return */
	{'u', kill_to_start},
	{'y', yank},
	{'w', kill_backward_word},
	{Up,	backward_line},
	{Down,	forward_line},
	{Right,	forward_word},
	{Left,	backward_word},
	{0, noop},
};
static struct keyconfig
keys_meta[] = {
	{'b', backward_word},
	{'f', forward_word},
	{0, noop},
};
static struct keyconfig
keys_meta_control[] = {
	{0, noop},
};
static struct keyconfig
keys_shift[] = {	/* only function keys */
	{0, noop},
};
static struct keyconfig
*keys[] = {
	keys_normal,
	keys_control,
	keys_meta,
	keys_meta_control,
	keys_shift,
};
	

static void
editor_rune(Editor e, Rune r){
	struct keyconfig *k;
	if(e->mode > sizeof(keys)/sizeof(*keys))
		e->mode = 0;
	for(k = keys[e->mode]; k->r!=0; k++)
		if(k->r == r) break;
	e->mode = Normal;
	k->func(e, r);
	e->kill_roll >>= 1;
}

Rune
function_key[] = {
	NoOp, Find, Ins, Del, Sel, Prior, Next, Home, End, NoOp, NoOp,
	NoOp, FKey+1, FKey+2, FKey+3, FKey+4, FKey+5,
	NoOp, FKey+6, FKey+7, FKey+8, FKey+9, FKey+10,
	NoOp, FKey+11, FKey+12, FKey+13, FKey+14,
	NoOp, FKey+15, FKey+16,
	NoOp, FKey+17, FKey+18, FKey+19, FKey+20,
};
Rune
arrow_key[] = { Up, Down, Right, Left };

void
editor_sequence(Editor e, Rune c){
	switch(e->seqmode){
	case 0:
		if(c == Escape)
			e->seqmode = Escape;
		else if(c < ' '){
			e->mode |= Control;
			c += '`';
		}
		break;
	case Escape:
		switch(c){
		case '[':
		case 'O':
			e->seqmode = c;
			break;
		default:
			e->mode |= Meta;
			e->seqmode = 0;
			editor_sequence(e, c);
			break;
		}
		return;
		break;
	case '[':
		/* Ps digits */
		if('0' <= c && c <= '9'){
			e->Ps *= 10;
			e->Ps += c - '0';
			return;
		}
		/* Arrow keys */
		if('a' <= c && c <= 'd'){
			e->mode |= Shift;
			c = arrow_key[c - 'a'];
		}else if('A' <= c && c <= 'D'){
			c = arrow_key[c - 'A'];
		}else switch(c){
		case 'Z': /* shift-tab */
			e->mode |= Shift|Control;
			c = 'i';
			break;
		/* function keys */
		case '@':
			e->mode |= Shift;
		case '^':
			e->mode |= Control;
			if(e->Ps < sizeof(function_key)/sizeof(Rune))
				c = function_key[e->Ps];
			break;
		case '$':
			e->mode |= Shift;
		case '~':
			if(e->Ps < sizeof(function_key)/sizeof(Rune))
				c = function_key[e->Ps];
			break;
		}
		e->Ps = 0;
		break;
	case 'O':
		/* Arrow keys with control */
		if('a' <= c && c <= 'd'){
			e->mode |= Control;
			c = arrow_key[c - 'a'];
		}else if('A' <= c && c <= 'A'){
			e->mode |= Shift|Control;
			c = arrow_key[c - 'A'];
		}
		break;
	}
	e->seqmode = 0;
	editor_rune(e, c);
}

void
editor_char(Editor e, char c){
	Rune r;
	int len;
	e->utf[e->utflen++] = c;
	e->utf[e->utflen] = '\0';
	len = rune_utf8(&r, e->utf);

	if(r == IncRune && len == e->utflen) return;

	editor_sequence(e, r);

	/* the character ended a still-incomplete multibyte rune */
	if(len < e->utflen){
		e->utflen = 0;
		editor_char(e, c);
	}
	e->utflen = 0;
}

void
editor(Editor e, int fd){
	int len;
	char buf[256], *b;
	len = read(fd, buf, sizeof(buf));
	if(len <= 0) return;

	for(b=buf;len > 0; len--){
		editor_char(e, *b++);
	}
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

