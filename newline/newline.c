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

double lines = 1;
int pty_mode = 0;

Text line;
Text yank;
int cursor;
int kill_roll;
char *history_file;

static Rune
fget_rune(FILE* f){
	static char utf[UTF8_MAX + 1];
	static int utflen;
	int len = 0;
	Rune r = IncRune;

	if(utflen)
		len = rune_of_utf8(&r, utf);
	while(r == IncRune && len == utflen){
		if((utf[utflen] = getc(f)) == EOF) return EOF;
		utf[++utflen]='\0';
		len = rune_of_utf8(&r, utf);
	}
	utflen -= len;
	memmove(utf, utf+len, utflen+1);
	return r;
}

static int
stdwrite(FILE* f, char *str, int len){
	if(fwrite(str, len, 1, f) != 1) exit(0);
	return len;
}

static void
cursor_shift(int n){
	char cmd[12];
	int len;
	if(n==0) return;
	len = snprintf(cmd, 12, "\e[%d%c", n, n<0? 'D' : 'C');
	stdwrite(stderr, cmd, len);
	cursor += n;
}

static void
erase_line(void){
	char cmd[]="\e[K";
	stdwrite(stderr, cmd, sizeof(cmd)-1);
}


static int
display_width(const Text t, int start, int end){
	int width = 0;
	for(;start < end;start++)
		width += rune_width(t->buf->r[start]);
	return width;
}
static Rune
display_rune(Rune r){
	if(r < ' ')	return 0x2400 | r;
	if(r == 0x7f)	return 0x2421;
	return r;
}

static char display_intro_replaced[] = "\x1b[1;32m";
static char display_intro_normal[] = "\x1b[0m";
enum { SPECIAL_MAX = sizeof(display_intro_replaced) + sizeof(display_intro_normal) };
static int
display_utf8_of_rune(char *u, Rune r){
	Rune d = display_rune(r);
	char *ue;
	if(d != r){
		ue = stpcpy(u, display_intro_replaced);
		ue += utf8_of_rune(ue, d);
		ue = stpcpy(ue, display_intro_normal);
		return ue-u;
	}
	return utf8_of_rune(u, d);
}

static void
render(FILE* f, const Runes* src, int (*utf8_from_rune)(char *, Rune)){
	int i, len;
	char buf[UTF8_MAX+SPECIAL_MAX];
	for(i=0;i<line->buf->c;i++){
		len = (*utf8_from_rune)(buf, src->r[i]);
		stdwrite(f, buf, len);
	}
}

static void
redraw_line(void){
	cursor_shift(-cursor);
	render(stderr, line->buf, display_utf8_of_rune);
	cursor += display_width(line, 0, line->buf->c);
	erase_line();
	cursor_shift(-display_width(line, line->off, line->buf->c));
}

static void
redraw_line_noecho(void){
	cursor_shift(line->off - cursor); /* invisible characters are all one space */
}

static void (*redraw_func)(void) = redraw_line;

static void
do_kill(int len){
	if(!kill_roll) text_clear(yank);
	kill_roll=2;
	if(len < 0){
		text_shift(line, len);
		len = -len;
		text_shift(yank, -yank->off);
	} else {
		text_shift(yank, yank->buf->c - yank->off);
	}
	text_insert(yank, line->buf->r + line->off, len);
	text_delete(line, len);
}

static void
do_shift(int n){
	text_shift(line, n);
}

static void
append_character(Rune c){
	text_append(line, &c, 1);
}

static void
insert_character(Rune c, void* f, int v){
	if(0xd800 <= c && c < 0xdc00) return;
	text_insert(line, &c, 1);
}

Runes** hist;
int hist_cur, hist_len, hist_sz, hist_first;

static void
history_save(){
	Runes* thisline;
	if(!line->buf->c || redraw_func != redraw_line) return;

	if((thisline = malloc(sizeof(*thisline) + line->buf->c * sizeof(Rune))) == NULL) return;
	memcpy(thisline, line->buf, sizeof(*thisline) + line->buf->c * sizeof(Rune));

	if(hist[hist_cur] != NULL)
		free(hist[hist_cur]);
	hist[hist_cur] = thisline;
}

static void
history_advance(int n){
	hist_cur += n;
	if(hist_cur < 0) hist_cur = 0;
	else if(hist_cur >= hist_len) hist_cur = hist_len - 1;
}

static void
history_load(){
	text_clear(line);
	if(hist[hist_cur] != NULL)
		text_insert(line, hist[hist_cur]->r, hist[hist_cur]->c);
}

static void
history_add(){
	hist_cur = hist_len;
	hist_len++;
	if(hist_len >= hist_sz){
		hist_sz = hist_len*2;
		hist = realloc(hist, sizeof(*hist)*hist_sz);
	}
	hist[hist_cur] = NULL;
}

static void
history_shift(Rune c, void* f, int n){
	if(n){
		history_save();
		history_advance(n);
	}
	history_load();
}

static void
history_dump(){
	FILE *f;
	char len[UTF8_MAX];
	int i;
	f = fopen(history_file, "a");
	if(!f) return;
	for(i = hist_first; i < hist_len; i++){
		if(hist[i] == NULL) continue; // FIXME should we save null entries?
		stdwrite(f, len, utf8_of_rune(len, hist[i]->c));
		render(f, hist[i], utf8_of_rune);
	}
	fclose(f);
}

static void
history_restore(){
	FILE *f;
	int len, i;
	if((f = fopen(history_file, "r")) == NULL) return;
	while(1){
		text_clear(line);
		if((len = fget_rune(f)) == EOF) break;
		for(i = 0; i<len; i++)
			insert_character(fget_rune(f), NULL, 0);
		history_save();
		history_add();
		hist_first++;
	}
	fclose(f);
}

static void
end(int n){
	if(line->buf->c == 0) lines = 0;
}

static void
flush_line(Rune c, void* f, int v){
	if(pty_mode || f == NULL){
		append_character(v);
	}else{
		(*(void(*)(int)) f)(v);
	}
	cursor_shift(-cursor);
	erase_line();
	fflush(stderr);
	render(stdout, line->buf, utf8_of_rune);
	fflush(stdout);
	if(pty_mode || f == NULL) line->buf->c--; /* FIXME! store history without the flushing character! */
	hist_cur = hist_len - 1;
	history_save();
	if(--lines <= 0) exit(0);
	history_add();
	history_load();
}

static void
set_echo(Rune c, void* v, int enable){
	if(!enable){
		redraw_func = redraw_line_noecho;
		cursor_shift(-cursor);
		erase_line();
	}else{
		redraw_func = redraw_line;
	}
}

static void
delete(Rune c, void* f, int d){
	text_delete(line, (*(int(*)(int)) f)(d));
}

static void
kill(Rune c, void* f, int d){
	do_kill((*(int(*)(int)) f)(d));
}

static void
shift(Rune c, void* f, int d){
	do_shift((*(int(*)(int)) f)(d));
}

static void
insert_yank(Rune c, void* v, int n){
	text_insert(line, yank->buf->r, yank->buf->c);
}

static Rune get_rune(void);
static void
insert_literal(Rune c, void* f, int v){
	insert_character(get_rune(), f, v);
}

/* distance functions */

static int
chars(int n){
	return n;
}

static int
words(int dir){
	Rune *r = line->buf->r;
	int i = line->off;
	if(dir < 0) i--;
	for(; i>=0 && i<line->buf->c && is_rune_space(r[i]); i+=dir);
	for(; i>=0 && i<line->buf->c && !is_rune_space(r[i]); i+=dir);
	if(dir < 0) i++;
	return i - line->off;
}

static int
to_the(int end){
	return end*line->buf->c - line->off;
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
	void (*func)(Rune, void*, int);
	int n;
	void *f;
};
static struct keyconfig keys_norm[], keys_ctrl[], keys_meta[],
			*mode = keys_norm;

static void
set_mode(Rune c, void *k, int n){
	mode = (struct keyconfig *)k;
}

static struct keyconfig
keys_norm[] = {
	{0x7f,	delete, -1, chars},
	{Del,	delete, 1, chars},
	{Up,	history_shift, -1},
	{Down,	history_shift, 1},
	{Right,	shift, 1, chars},
	{Left,	shift, -1, chars},
	{Home,	shift, 0, to_the},
	{End,	shift, 1, to_the},
	{Ctrl,	set_mode, 0, keys_ctrl},
	{Meta,	set_mode, 0, keys_meta},
	{0, insert_character},
};

static struct keyconfig
keys_ctrl[] = {
	{'a', shift, 0, to_the},
	{'b', shift, -1, chars},
	{'c', flush_line, '\x03', exit},
	{'d', flush_line, '\x04', end},
	{'e', shift, 1, to_the},
	{'f', shift, 1, chars},
	{'h', kill, -1, chars},
	{'k', kill, 1, to_the},
	{'m', flush_line, '\x0a'},	/* Return */
	{'n', history_shift, 1},
	{'p', history_shift, -1},
	{'q', set_echo, 1},
	{'s', set_echo, 0},
	{'u', kill, 0, to_the},
	{'v', insert_literal},
	{'y', insert_yank},
	{'w', kill, -1, words},
	{'z', history_shift, 0},
	{Up,	history_shift, 0},
	{Down,	history_shift, -1},
	{Right,	shift, 1, words},
	{Left,	shift, -1, words},
	{0, set_mode, 0, keys_norm},
};

static struct keyconfig
keys_meta[] = {
	{'b', shift, -1, words},
	{'f', shift, 1, words},
	{0, set_mode, 0, keys_norm},
};

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
		len = rune_of_utf8(&r, utf);
	while(r == IncRune && len == utflen){
		if((utf[utflen] = getchar()) == EOF) exit(0);
		utf[++utflen]='\0';
		len = rune_of_utf8(&r, utf);
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
		set_mode(r, keys_norm, 0);
		k->func(r, k->f, k->n);
		if(mode == keys_norm)
			kill_roll >>= 1;
	}
}

static void
newline_cleanup(void){
	if(line) free(line);
	if(yank) free(yank);
}

static int
init(void){
	if((line = text_new(Linesize)) == NULL) return 0;
	if((yank = text_new(Linesize)) == NULL) return 0;
	atexit(&newline_cleanup);
	setvbuf(stderr, NULL, _IOFBF, BUFSIZ);
	history_add();
	history_restore();
	atexit(&history_dump);
	return 1;
}

int
newline(void){
	if(!init()) return 1;
	while(1){
		handle_rune(get_rune());
		(*redraw_func)();
		fflush(stderr);
	}
}

