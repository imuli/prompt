#ifndef EDITOR_H
#define EDITOR_H
typedef struct {
	Buffer pty;
	Buffer term;
	Text line;
	int echo;
	Text yank;
	int kill_roll;
	int cursor;
	void *mode;
}* Editor;

void editor(Editor e, int fd);
Editor editor_init(Buffer pty, Buffer term);
#endif
