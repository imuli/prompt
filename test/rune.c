#include <stdio.h>
#include <string.h>
#include "../rune.h"

static struct { Rune r; char u[UTF8_MAX+1]; int len; }
utf8_tests[] = {
	{ 0x0000, "\x00", 1 },
	{ 0x007f, "\x7f", 1 },
	{ 0x0080, "\xc2\x80", 2 },
	{ 0x07ff, "\xdf\xbf", 2 },
	{ 0x0800, "\xe0\xa0\x80", 3 },
	{ 0xffff, "\xef\xbf\xbf", 3 },
	{ 0x10000, "\xf0\x90\x80\x80", 4 },
	{ 0x1fffff, "\xf7\xbf\xbf\xbf", 4 },
	{ 0x200000, "\xf8\x88\x80\x80\x80", 5 },
	{ 0x3ffffff, "\xfb\xbf\xbf\xbf\xbf", 5 },
	{ 0x4000000, "\xfc\x84\x80\x80\x80\x80", 6 },
	{ 0x7fffffff, "\xfd\xbf\xbf\xbf\xbf\xbf", 6 },
	{ 0x80000000, "\xfe\x82\x80\x80\x80\x80\x80", 7 },
	{ 0xffffffff, "\xfe\x83\xbf\xbf\xbf\xbf\xbf", 7 },
};

int
test_runechar(void){
	int i=0, len, pass=0, fail=0;
	do{
		len = runechar(utf8_tests[i].r);
		if(len != utf8_tests[i].len){
			fail++;
			fprintf(stderr, "runechar test #%d failed: %d\n", i, len);
		} else
			pass++;
	}while(utf8_tests[i++].r != RUNE_MAX);
	fprintf(stderr, "runechar %d tests passed\n", pass);
	return fail;
}

int
test_utf8char(void){
	int i=0, len, pass=0, fail=0;
	do{
		len = utf8char(utf8_tests[i].u);
		if(len != utf8_tests[i].len){
			fail++;
			fprintf(stderr, "utf8char test #%d failed: %d\n", i, len);
		} else
			pass++;
	}while(utf8_tests[i++].r != RUNE_MAX);
	fprintf(stderr, "utf8char %d tests passed\n", pass);
	return fail;
}

int
test_rune_utf8(void){
	int i=0, len, pass=0, fail=0;
	Rune r;
	do{
		len = rune_utf8(&r, utf8_tests[i].u);
		if(len != utf8_tests[i].len || r != utf8_tests[i].r){
			fail++;
			fprintf(stderr, "rune_utf8 test #%d failed: %d, %x\n", i, len, r);
		} else
			pass++;
	}while(utf8_tests[i++].r != RUNE_MAX);
	fprintf(stderr, "rune_utf8 %d tests passed\n", pass);
	return fail;
}

int
test_utf8_rune(void){
	int i=0, len, pass=0, fail=0;
	char buf[UTF8_MAX+1];
	do{
		len = utf8_rune(buf, utf8_tests[i].r);
		if(len != utf8_tests[i].len ||
				memcmp(buf, utf8_tests[i].u, len) != 0){
			fail++;
			fprintf(stderr, "utf8_rune test #%d failed: %d %s\n", i, len, buf);
		} else
			pass++;
	}while(utf8_tests[i++].r != RUNE_MAX);
	fprintf(stderr, "utf8_rune %d tests passed\n", pass);
	return fail;
}

static struct { Rune r[5]; int rlen; char u[11]; int ulen; }
utf8s_tests[] = {
	{ {0x61, 0x03b1, 0x920, 0x1f700, 0x00}, 4, "aαठ🜀", 10 },
	{ {0x00}, 0, "", 0 },
};

int
test_runeschars(void){
	int i=0, len, pass=0, fail=0;
	do{
		len = runeschars(utf8s_tests[i].r);
		if(len != utf8s_tests[i].ulen){
			fail++;
			fprintf(stderr, "runeschars test #%d failed: %d\n", i, len);
		} else
			pass++;
		i++;
	}while(utf8s_tests[i].rlen != 0);
	fprintf(stderr, "runeschars %d tests passed\n", pass);
	return fail;
}

int
test_utf8srunes(void){
	int i=0, len, pass=0, fail=0;
	do{
		len = utf8srunes(utf8s_tests[i].u);
		if(len != utf8s_tests[i].rlen){
			fail++;
			fprintf(stderr, "utf8srunes test #%d failed: %d\n", i, len);
		} else
			pass++;
		i++;
	}while(utf8s_tests[i].rlen != 0);
	fprintf(stderr, "utf8srunes %d tests passed\n", pass);
	return fail;
}

int
test_runes_utf8s(void){
	int i=0, len, pass=0, fail=0;
	Rune r[10];
	do{
		len = runes_utf8s(r, utf8s_tests[i].u);
		if(len != utf8s_tests[i].rlen &&
				memcmp(r, utf8s_tests[i].r, len*sizeof(Rune))){
			fail++;
			fprintf(stderr, "runes_utf8s test #%d failed: %d\n", i, len);
		} else
			pass++;
		i++;
	}while(utf8s_tests[i].rlen != 0);
	fprintf(stderr, "runes_utf8s %d tests passed\n", pass);
	return fail;
}

int
test_utf8s_runes(void){
	int i=0, len, pass=0, fail=0;
	char u[20];
	do{
		len = utf8s_runes(u, utf8s_tests[i].r);
		if(len != utf8s_tests[i].ulen &&
				memcmp(u, utf8s_tests[i].u, len)){
			fail++;
			fprintf(stderr, "utf8s_runes test #%d failed: %d\n", i, len);
		} else
			pass++;
		i++;
	}while(utf8s_tests[i].rlen != 0);
	fprintf(stderr, "utf8s_runes %d tests passed\n", pass);
	return fail;
}

int
main(int argc, char **argv){
	int fail = 0;
	fail += test_runechar();
	fail += test_utf8char();
	fail += test_rune_utf8();
	fail += test_utf8_rune();

	fail += test_runeschars();
	fail += test_utf8srunes();
	fail += test_runes_utf8s();
	fail += test_utf8s_runes();
	return fail > 0;
}
