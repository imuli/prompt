#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rune.h"
#include "text.h"
#include "newline.h"

enum {
	Linesize = 128,
};

Text line;
Text yank;
int loop = 1;
int cursor;
int kill_roll;
int echo = 1;

static void
cursor_shift(int n){
	char cmd[12];
	int len;
	if(n==0) return;
	len = snprintf(cmd, 12, "\e[%d%c", n, n<0? 'D' : 'C');
	newline_out(cmd, len);
	cursor += n;
}

static void
erase_line(void){
	char cmd[]="\e[K";
	newline_out(cmd, sizeof(cmd)-1);
}

static int
word_len(int dir){
	Rune *r = line->buf;
	int i = line->off;
	if(dir < 0) i--;
	for(; i>=0 && i<line->len && rune_isspace(r[i]); i+=dir);
	for(; i>=0 && i<line->len && !rune_isspace(r[i]); i+=dir);
	if(dir < 0) i++;
	return i - line->off;
}

static void
kill(int len){
	if(!kill_roll) text_clear(yank);
	kill_roll=2;
	if(len < 0){
		text_shift(line, len);
		len = -len;
		text_shift(yank, -yank->off);
	} else {
		text_shift(yank, yank->len - yank->off);
	}
	text_insert(yank, line->buf + line->off, len);
	text_delete(line, len);
}

static void
shift(int n){
	cursor_shift(text_shift(line, n));
}

static void
redraw_line(Rune c){
	if(!echo)
		return cursor_shift(line->off-cursor);
	cursor_shift(-cursor);
	text_render(line);
	newline_out(line->text, line->textlen);
	cursor += line->len;
	erase_line();
	cursor_shift(line->off - line->len);
}

static void
append_character(Rune c){
	text_append(line, &c, 1);
}

static void
insert_character(Rune c){
	if(0xd800 <= c && c < 0xdc00) return;
	text_insert(line, &c, 1);
	redraw_line(c);
}

static void
flush_line(Rune c){
	cursor_shift(-line->off);
	erase_line();
	text_render(line);
	newline_lineout(line->text, line->textlen);
	text_clear(line);
}

static void
backward_line(Rune c){
	shift(-line->off);
}

static void
backward_word(Rune c){
	shift(word_len(-1));
}

static void
backward_char(Rune c){
	shift(-1);
}

static void
forward_word(Rune c){
	shift(word_len(1));
}

static void
forward_line(Rune c){
	shift(line->len - line->off);
}

static void
forward_char(Rune c){
	shift(1);
}

static void
interrupt(Rune c){
	text_clear(line);
	append_character(c);
	flush_line(c);
}

static void
end_of_file(Rune c){
	append_character(4);
	flush_line(c);
}

static void
backspace_char(Rune c){
	text_delete(line, -1);
	redraw_line(c);
}

static void
kill_to_end(Rune c){
	kill(line->len - line->off);
	redraw_line(c);
}

static void
send_line(Rune c){
	append_character('\r');
	flush_line(c);
}

static void
kill_to_start(Rune c){
	kill(-line->off);
	redraw_line(c);
}

static void
kill_backward_word(Rune c){
	kill(word_len(-1));
	redraw_line(c);
}

static void
kill_backward_char(Rune c){
	kill(-1);
	redraw_line(c);
}

static void
insert_yank(Rune c){
	text_insert(line, yank->buf, yank->len);
	redraw_line(c);
}

enum {
	/* special keys present in unicode */
	Escape	= 0x1b,
	Bksp	= 0x7f,
	/* surrogates should not appear in valid unicode streams,
	 * so are safe to use for function key definitions.
	 */
	Base	= 0xd7ff,
	Normal,
	Ctrl,
	Meta,
	Shift,
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

/* keyboard configuration */
struct keyconfig {
	Rune r;
	void (*func)(Rune);
};

static void mode_norm(Rune);
static void mode_ctrl(Rune);
static void mode_meta(Rune);

static struct keyconfig
keys_norm[] = {
	{0x7f, backspace_char},
	{Right,	forward_char},
	{Left,	backward_char},
	{Home,	backward_line},
	{End,	forward_line},
	{Ctrl,	mode_ctrl},
	{Meta,	mode_meta},
	{0, insert_character},
};

static struct keyconfig
keys_ctrl[] = {
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
	{'y', insert_yank},
	{'w', kill_backward_word},
	{Up,	backward_line},
	{Down,	forward_line},
	{Right,	forward_word},
	{Left,	backward_word},
	{0, mode_norm},
};

static struct keyconfig
keys_meta[] = {
	{'b', backward_word},
	{'f', forward_word},
	{0, mode_norm},
};

struct keyconfig *mode = keys_norm;
static void mode_norm(Rune c){ mode = keys_norm; }
static void mode_ctrl(Rune c){ mode = keys_ctrl; }
static void mode_meta(Rune c){ mode = keys_meta; }

/* input handling */
static void handle_rune(Rune);
static Rune get_rune(void);

Rune
function_key[] = {
	Normal, Find, Ins, Del, Sel, Prior, Next, Home, End, Normal, Normal,
	Normal, FKey+1, FKey+2, FKey+3, FKey+4, FKey+5,
	Normal, FKey+6, FKey+7, FKey+8, FKey+9, FKey+10,
	Normal, FKey+11, FKey+12, FKey+13, FKey+14,
	Normal, FKey+15, FKey+16,
	Normal, FKey+17, FKey+18, FKey+19, FKey+20,
};

static Rune
get_fkey(int Ps){
	if(Ps < sizeof(function_key)/sizeof(Rune))
		return function_key[Ps];
	return Normal;
}

static Rune
get_esc_bra_ps(Rune r){
	int Ps = 0;
	while('0' < r && r <= '9'){
		Ps *= 10;
		Ps += r - '0';
		r = get_rune();
	}
	switch(r){
	case '@': handle_rune(Ctrl); handle_rune(Shift); return get_fkey(Ps);
	case '^': handle_rune(Ctrl); return get_fkey(Ps);
	case '$': handle_rune(Shift); return get_fkey(Ps);
	case '~': return get_fkey(Ps);
	case ';': /* return get_esc_bra_ps_pt(Ps) */
	default: return Normal;
	}
}

static Rune
get_esc_bra(void){
	Rune r = get_rune();
	if('0' <= r && r <= '9')
		return get_esc_bra_ps(r);
	switch(r){
	case 'A': return Up;
	case 'B': return Down;
	case 'C': return Right;
	case 'D': return Left;
	case 'a': handle_rune(Shift); return Up;
	case 'b': handle_rune(Shift); return Down;
	case 'c': handle_rune(Shift); return Right;
	case 'd': handle_rune(Shift); return Left;
	case 'Z': handle_rune(Shift); handle_rune(Ctrl); return 'i';
	default: return Normal;
	}
}
		
static Rune
get_esc_O(void){
	Rune r = get_rune();
	switch(r){
	case 'a': handle_rune(Ctrl); return Up;
	case 'b': handle_rune(Ctrl); return Down;
	case 'c': handle_rune(Ctrl); return Right;
	case 'd': handle_rune(Ctrl); return Left;
	case 'A': handle_rune(Ctrl); handle_rune(Shift); return Up;
	case 'B': handle_rune(Ctrl); handle_rune(Shift); return Down;
	case 'C': handle_rune(Ctrl); handle_rune(Shift); return Right;
	case 'D': handle_rune(Ctrl); handle_rune(Shift); return Left;
	default: return Normal;
	}
}

static Rune
get_escape(void){
	Rune r = get_rune();
	switch(r){
	case '[':
		return get_esc_bra();
	case 'O':
		return get_esc_O();
	default:
		handle_rune(Meta);
		return r;
	}
}

static Rune
get_rune(void){
	static char utf[UTF8_MAX + 1];
	static int utflen;
	int len = 0;
	Rune r = IncRune;

	if(utflen)
		len = rune_utf8(&r, utf);
	while(r == IncRune && len == utflen){
		newline_in(utf+utflen, 1);
		utf[++utflen]='\0';
		len = rune_utf8(&r, utf);
	}
	utflen -= len;
	memmove(utf, utf+len, utflen+1);
	return r;
}

static void
handle_rune(Rune r){
	struct keyconfig *k;
	if(r == Escape){
		handle_rune(get_escape());
	}else if(r < ' '){
		handle_rune(Ctrl);
		handle_rune(r | 0x60);
	}else{
		for(k=mode; k->r!=0; k++)
			if(k->r == r) break;
		mode_norm(r);
		k->func(r);
		kill_roll >>= 1;
	}
}

static int
init(void){
	if((line = text_new(Linesize)) == NULL) return 0;
	if((yank = text_new(Linesize)) == NULL) return 0;
	return 1;
}

void
newline_cleanup(void){
	if(line) free(line);
	if(yank) free(yank);
}

void
newline(void){
	if(!init()) return;
	while(loop)
		handle_rune(get_rune());
	newline_cleanup();
}

