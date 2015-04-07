#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rune.h"
#include "text.h"

void *
text_free(Text t){
	if(t == NULL) return NULL;
	if(t->buf) free(t->buf);
	if(t->text) free(t->text);
	free(t);
	return NULL;
}

Text
text_new(int size){
	Text t;
	if((t = calloc(1, sizeof(*t))) == NULL) return text_free(t);
	/* t->off = t->len = 0; */
	if((t->buf = malloc(size*sizeof(Rune))) == NULL) return text_free(t);
	t->sz = size;
	t->textsz = 0;
	return t;
}

static int
text_grow(Text t, int len){
	int n;
	for(n=t->sz; n<len+t->len+1; n*=2);
	if(n==t->sz) return 1;
	Rune *p = realloc(t->buf, 2*t->sz*sizeof(Rune));
	if(p == NULL) return 0;
	t->buf = p;
	return 1;
}

int
text_insert(Text t, Rune *s, int len){
	if(!text_grow(t, len)) return 0;
	memmove(t->buf + t->off + len, t->buf + t->off,
			(t->len - t->off) * sizeof(Rune));
	memcpy(t->buf + t->off, s, len * sizeof(Rune));
	t->off += len;
	t->len += len;
	return 1;
}

int
text_append(Text t, Rune *s, int len){
	if(!text_grow(t, len)) return 0;
	memcpy(t->buf + t->len, s, len * sizeof(Rune));
	t->len += len;
	return 1;
}

int
text_shift(Text t, int n){
	if(t->off + n < 0)
		n = -t->off;
	else if(t->off + n > t->len)
		n = t->len - t->off;
	t->off += n;
	return n;
}

int
text_clear(Text t){
	t->off = t->len = 0;
	return 1;
}

int
text_delete(Text t, int len){
	/* negative len means delete to the left */
	if(len < 0){
		t->off += len;
		len = -len;
	}
	if(t->off < 0){
		len -= t->off;
		t->off = 0;
	}
	if(t->off + len > t->len){
		len = t->len - t->off;
	}
	memmove(t->buf + t->off, t->buf + t->off + len,
			(t->len - t->off - len) * sizeof(Rune));
	t->len -= len;
	return len;
}

int
text_render(Text t){
	t->buf[t->len] = 0;
	int n = runeschars(t->buf);
	if(t->textsz < n+1){
		if(t->text)
			free(t->text);
		t->text = malloc(n*3/2+1);
		if(t->text == NULL) return 0;
		t->textsz = n*3/2+1;
	}
	utf8s_runes(t->text, t->buf);
	t->textlen = n;
	return n;
}

