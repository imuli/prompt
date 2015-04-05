typedef struct {
	Buffer pty;
	Buffer term;
	char *line;
	int offset;
	int echo;
}* Editor;

void editor(Editor e, int fd);
Editor editor_init(Buffer pty, Buffer term);

