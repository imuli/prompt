typedef struct {
	char* buf;
	int start, end;
}* Buffer;

Buffer buffer_alloc();
void * buffer_free(Buffer b);
int buffer_pull(Buffer b, int fd);
int buffer_push(int fd, Buffer b);
int buffer_add(Buffer b, const char *data, int sz);
int buffer_space(Buffer b);
int buffer_amount(Buffer b);

