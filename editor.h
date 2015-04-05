typedef struct {
	Buffer pty;
	Buffer term;
	char *line;
	int off, len;
	int echo;
}* Editor;

void editor(Editor e, int fd);
Editor editor_init(Buffer pty, Buffer term);

