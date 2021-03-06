#ifndef TEXT_H
#define TEXT_H
typedef struct {
	int off, sz;
	Runes* buf;
}* Text;

void* text_free(Text t);
Text text_new(int size);
int text_insert(Text t, Rune *s, int len);
int text_append(Text t, Rune *s, int len);
int text_shift(Text t, int n);
int text_clear(Text t);
int text_delete(Text t, int len);
#endif
