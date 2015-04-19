#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rune.h"
#include "text.h"
#include "newline.h"

enum {
	Linesize = 128,
	HistorySize = 128,
};

double lines;

Text line;
Text yank;
int cursor;
int kill_roll;

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
display_utf8_rune(char *u, Rune r){
	static int replaced;
	Rune d = display_rune(r);
	char *ue;
	if(d != r){
		ue = stpcpy(u, display_intro_replaced);
		ue += utf8_rune(ue, d);
		ue = stpcpy(ue, display_intro_normal);
		return ue-u;
	}
	return utf8_rune(u, d);
}

static void
render(FILE* f, int (*utf8_from_rune)(char *, Rune)){
	int i, len;
	char buf[UTF8_MAX+SPECIAL_MAX];
	for(i=0;i<line->buf->c;i++){
		len = (*utf8_from_rune)(buf, line->buf->r[i]);
		stdwrite(f, buf, len);
	}
}

static void
redraw_line(void){
	cursor_shift(-cursor);
	render(stderr, display_utf8_rune);
	cursor += display_width(line, 0, line->buf->c);
	erase_line();
	cursor_shift(-display_width(line, line->off, line->buf->c));
}

static void
redraw_line_noecho(void){
	cursor_shift(line->off - cursor); /* invisible characters are all one space */
}

static void (*redraw_func)(void) = redraw_line;

static int
word_len(int dir){
	Rune *r = line->buf->r;
	int i = line->off;
	if(dir < 0) i--;
	for(; i>=0 && i<line->buf->c && rune_isspace(r[i]); i+=dir);
	for(; i>=0 && i<line->buf->c && !rune_isspace(r[i]); i+=dir);
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
		text_shift(yank, yank->buf->c - yank->off);
	}
	text_insert(yank, line->buf->r + line->off, len);
	text_delete(line, len);
}

static void
shift(int n){
	text_shift(line, n);
}

static void
append_character(Rune c){
	text_append(line, &c, 1);
}

static void
insert_character(Rune c){
	if(0xd800 <= c && c < 0xdc00) return;
	text_insert(line, &c, 1);
}

Runes* hist;
int hist_cur, hist_len, hist_sz;

static void
history_save(){
	Runes thisline;
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
history_shift(int n){
	history_save();
	history_advance(n);
	history_load();
}

static void
history_previous(Rune c){
	history_shift(-1);
}

static void
history_this(Rune c){
	history_load();
}

static void
history_next(Rune c){
	history_shift(1);
}

static void
flush_line(Rune c){
	append_character(c);
	cursor_shift(-cursor);
	erase_line();
	fflush(stderr);
	render(stdout, utf8_rune);
	fflush(stdout);
	if(--lines <= 0) exit(0);
	line->buf->c--; /* FIXME! store history without the flushing character! */
	hist_cur = hist_len - 1;
	history_save();
	history_add();
	history_load();
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
echo_off(Rune c){
	redraw_func = redraw_line_noecho;
	cursor_shift(-cursor);
	erase_line();
}

static void
echo_on(Rune c){
	redraw_func = redraw_line;
}

static void
forward_word(Rune c){
	shift(word_len(1));
}

static void
forward_line(Rune c){
	shift(line->buf->c - line->off);
}

static void
forward_char(Rune c){
	shift(1);
}

static void
interrupt(Rune c){
	text_clear(line);
	flush_line(3);
}

static void
end_of_file(Rune c){
	flush_line(4);
}

static void
delete_backward_char(Rune c){
	text_delete(line, -1);
}

static void
delete_forward_char(Rune c){
	text_delete(line, 1);
}

static void
kill_to_end(Rune c){
	kill(line->buf->c - line->off);
}

static void
send_line(Rune c){
	flush_line('\n');
}

static void
kill_to_start(Rune c){
	kill(-line->off);
}

static void
kill_backward_word(Rune c){
	kill(word_len(-1));
}

static void
kill_backward_char(Rune c){
	kill(-1);
}

static void
insert_yank(Rune c){
	text_insert(line, yank->buf->r, yank->buf->c);
}

static Rune get_rune(void);
static void
insert_literal(Rune c){
	insert_character(get_rune());
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
	{0x7f,	delete_backward_char},
	{Del,	delete_forward_char},
	{Up,	history_previous},
	{Down,	history_next},
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
	{'m', send_line},	/* Return */
	{'n', history_next},
	{'p', history_previous},
	{'q', echo_on},
	{'s', echo_off},
	{'u', kill_to_start},
	{'v', insert_literal},
	{'y', insert_yank},
	{'w', kill_backward_word},
	{'z', history_this},
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
		if((utf[utflen] = getchar()) == EOF) exit(0);
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

