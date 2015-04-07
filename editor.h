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
	int mode;
	char utf[UTF8_MAX+1];
	int utflen;
	int seqmode;
	int Ps;
}* Editor;

void editor(Editor e, int fd);
Editor editor_init(Buffer pty, Buffer term);
#endif
