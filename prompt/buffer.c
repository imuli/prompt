#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"

enum {
	Bsize = 1024,
};

static void
buffer_normalize(Buffer b){
	if(b->start != 0){
		b->start %= Bsize;
		b->end %= Bsize;
	}
	if(b->start == b->end){
		b->start = b->end = 0;
	}
}

int
buffer_space(Buffer b){
	return (b->start > b->end ? b->start : Bsize) - b->end;
}

int
buffer_amount(Buffer b){
	return (b->start > b->end ? Bsize : b->end) - b->start;
}

Buffer
buffer_alloc(){
	Buffer b;
	if((b = calloc(1, sizeof(*b))) == NULL)
		return NULL;
	if((b->buf = malloc(Bsize)) == NULL)
		return buffer_free(b);
	return b;
}

void *
buffer_free(Buffer b){
	if(b){
		if(b->buf)
			free(b->buf);
		free(b);
	}
	return NULL;
}

int
buffer_pull(Buffer b, int fd){
	int len;
	buffer_normalize(b);
	len = read(fd, b->buf + b->end, buffer_space(b));
	if(len > 0)
		b->end += len;
	buffer_normalize(b);
	return len;
}

int
buffer_push(int fd, Buffer b){
	int len;
	buffer_normalize(b);
	len = write(fd, b->buf + b->start, buffer_amount(b));
	if(len > 0)
		b->start += len;
	buffer_normalize(b);
	return len;
}

int
buffer_add(Buffer b, const char *data, int sz){
	int after = buffer_space(b);
	int before = (b->start > b->end ? 0 : b->start);
	buffer_normalize(b);
	if(sz > after + before)
		return after + before - sz;
	if(sz > after){
		memcpy(b->buf + b->end, data, after);
		memcpy(b->buf, data, sz - after);
	}else{
		memcpy(b->buf + b->end, data, sz);
	}
	b->end += sz;
	buffer_normalize(b);
	return sz;
}

