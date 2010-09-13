/* Generate a d8 table for a given character set. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>

int
main(int argc, char *argv[])
{
    iconv_t iu;
    int i;
    unsigned short d8[256];
    char inbuf[2];
    unsigned short outbuf[2];
    char *inbufp;
    char *outbufp;
    size_t inbytesleft;
    size_t outbytesleft;

    if (argc != 2) {
	fprintf(stderr, "usage: %s csname\n", argv[0]);
	exit(1);
    }
    iu = iconv_open("UCS-2LE", argv[1]);
    if (iu == (iconv_t)-1) {
	fprintf(stderr, "no converter\n");
	exit(1);
    }
    memset(d8, '\0', sizeof(d8));
    for (i = 0x20; i <= 0xff; i++) {
	size_t nc;

	inbuf[0] = i;
	inbuf[1] = '\0';
	inbufp = (char *)&inbuf;
	inbytesleft = 1;
	outbufp = (char *)&outbuf;
	outbytesleft = sizeof(outbuf);
	nc = iconv(iu, &inbufp, &inbytesleft, &outbufp, &outbytesleft);
	if (nc >= 0) {
	    d8[i] = outbuf[0];
	}
    }

    for (i = 0; i <= 0xff; i++) {
	if (i && !(i % 4))
	    printf("\n");
	if (!(i % 4))
	    printf("\t");
	printf("0x%08x, ", d8[i]);
    }
    printf("\n");
    return 0;
}
