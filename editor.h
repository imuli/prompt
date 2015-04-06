#ifndef EDITOR_H
#define EDITOR_H
typedef struct {
	Buffer pty;
	Buffer term;
	Text line;
	int echo;
}* Editor;

void editor(Editor e, int fd);
Editor editor_init(Buffer pty, Buffer term);
#endif
