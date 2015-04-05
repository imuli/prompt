#include <stdio.h>
#include <termios.h>

struct flaginfo {
	tcflag_t flag;
	char name[12];
};
const struct flaginfo iflags[] = {
	{ IGNBRK, "IGNBRK" },
	{ BRKINT, "BRKINT" },
	{ IGNPAR, "IGNPAR" },
	{ PARMRK, "PARMRK" },
	{ INPCK, "INPCK" },
	{ ISTRIP, "ISTRIP" },
	{ INLCR, "INLCR" },
	{ IGNCR, "IGNCR" },
	{ ICRNL, "ICRNL" },
	{ IUCLC, "IUCLC" },
	{ IXON, "IXON" },
	{ IXANY, "IXANY" },
	{ IXOFF, "IXOFF" },
	{ IMAXBEL, "IMAXBEL" },
	{ IUTF8, "IUTF8" },
	{ 0, "" },
};
const struct flaginfo oflags[] = {
	{ OPOST, "OPOST" },
	{ OLCUC, "OLCUC" },
	{ ONLCR, "ONLCR" },
	{ OCRNL, "OCRNL" },
	{ ONOCR, "ONOCR" },
	{ ONLRET, "ONLRET" },
	{ OFILL, "OFILL" },
	{ OFDEL, "OFDEL" },
	{ NLDLY, "NLDLY" },
	{ CRDLY, "CRDLY" },
	{ TABDLY, "TABDLY" },
	{ BSDLY, "BSDLY" },
	{ VTDLY, "VTDLY" },
	{ FFDLY, "FFDLY" },
	{ 0, "" },
};
const struct flaginfo cflags[] = {
/* multi-bit and probably won't change anyway
	{ CBAUD, "CBAUD" },
	{ CBAUDEX, "CBAUDEX" },
	{ CSIZE, "CSIZE" },
*/
	{ CSTOPB, "CSTOPB" },
	{ CREAD, "CREAD" },
	{ PARENB, "PARENB" },
	{ HUPCL, "HUPCL" },
	{ CLOCAL, "CLOCAL" },
#ifdef LOBLK
	{ LOBLK, "LOBLK" },
#endif
	{ CIBAUD, "CIBAUD" },
	{ CMSPAR, "CMSPAR" },
	{ CRTSCTS, "CRTSCTS" },
	{ 0, "" },
};
const struct flaginfo lflags[] = {
	{ ISIG, "ISIG" },
	{ ICANON, "ICANON" },
	{ XCASE, "XCASE" },
	{ ECHO, "ECHO" },
	{ ECHOE, "ECHOE" },
	{ ECHOK, "ECHOK" },
	{ ECHONL, "ECHONL" },
	{ ECHOCTL, "ECHOCTL" },
	{ ECHOPRT, "ECHOPRT" },
	{ ECHOKE, "ECHOKE" },
#ifdef DEFECHO
	{ DEFECHO, "DEFECHO" },
#endif
#ifdef FLUSHD
	{ FLUSHD, "FLUSHD" },
#endif
	{ NOFLSH, "NOFLSH" },
	{ TOSTOP, "TOSTOP" },
	{ PENDIN, "PENDIN" },
	{ IEXTEN, "IEXTEN" },
	{ 0, "" },
};

static int
compare_flags(tcflag_t old, tcflag_t new, const struct flaginfo *info){
	int n = 0;
	tcflag_t diff = old ^ new;
	for(;info->flag!=0; info++){
		if(diff & info->flag){
			fprintf(stderr, "%c%s ", 
				old & info->flag ? '-' : '+',
				info->name);
			n++;
		}
	}
	return n;
}

void
compare_termios(struct termios* termp, struct termios* termq){
	int n = 0;
	n += compare_flags(termp->c_iflag, termq->c_iflag, iflags);
	n += compare_flags(termp->c_oflag, termq->c_oflag, oflags);
	n += compare_flags(termp->c_cflag, termq->c_cflag, cflags);
	n += compare_flags(termp->c_lflag, termq->c_lflag, lflags);
	if(n) fprintf(stderr, "\n");
}
