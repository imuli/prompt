#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rune.h"
#include "text.h"

void *
text_free(Text t){
	if(t == NULL) return NULL;
	if(t->buf) free(t->buf);
	free(t);
	return NULL;
}

Text
text_new(int size){
	Text t;
	if((t = calloc(1, sizeof(*t))) == NULL) return text_free(t);
	/* t->off = 0; */
	if((t->buf = malloc(sizeof(*t->buf) + size*sizeof(Rune))) == NULL) return text_free(t);
	t->buf->c = 0;
	t->sz = size;
	return t;
}

static int
text_grow(Text t, int len){
	int n;
	for(n=t->sz; n<len+t->buf->c+1; n*=2);
	if(n==t->sz) return 1;
	void *p = realloc(t->buf, sizeof(*t->buf) + 2*t->sz*sizeof(Rune));
	if(p == NULL) return 0;
	t->buf = p;
	return 1;
}

int
text_insert(Text t, Rune *s, int len){
	if(!text_grow(t, len)) return 0;
	memmove(t->buf->r + t->off + len, t->buf->r + t->off,
			(t->buf->c - t->off) * sizeof(Rune));
	memcpy(t->buf->r + t->off, s, len * sizeof(Rune));
	t->off += len;
	t->buf->c += len;
	return 1;
}

int
text_append(Text t, Rune *s, int len){
	if(!text_grow(t, len)) return 0;
	memcpy(t->buf->r + t->buf->c, s, len * sizeof(Rune));
	t->buf->c += len;
	return 1;
}

int
text_shift(Text t, int n){
	if(t->off + n < 0)
		n = -t->off;
	else if(t->off + n > t->buf->c)
		n = t->buf->c - t->off;
	t->off += n;
	return n;
}

int
text_clear(Text t){
	t->off = t->buf->c = 0;
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
	if(t->off + len > t->buf->c){
		len = t->buf->c - t->off;
	}
	memmove(t->buf->r + t->off, t->buf->r + t->off + len,
			(t->buf->c - t->off - len) * sizeof(Rune));
	t->buf->c -= len;
	return len;
}

