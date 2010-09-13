#include <stdio.h>
#include <iconv.h>

#define PER_ROW	128
unsigned short u2gb[65536];

int
main(int argc, char *argv[])
{
    iconv_t i;
    int j;

    i = iconv_open("GB18030", "UCS2" /* BE */);
    if (i == (iconv_t)-1) {
	fprintf(stderr, "iconv_open failed\n");
	return 1;
    }

    for (j = 0; j <= 0xffff; j++) {
	char ucs2[2];
	char gb18030[16];
	char *inbuf;
	char *outbuf;
	size_t inbytesleft;
	size_t outbytesleft;
	size_t nc;

	/* iconv's UCS2 is little-endian */
	ucs2[0] = j & 0xff;
	ucs2[1] = (j >> 8) & 0xff;

	inbuf = ucs2;
	outbuf = gb18030;
	inbytesleft = 2;
	outbytesleft = 16;

	nc = iconv(i, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	if (inbytesleft == 2) {
	    /*printf("U+%04x does not convert\n", j);*/
	} else if (outbytesleft != 14) {
	    /*printf("U+%04x converts to %d-byte GB\n", j, 16 - outbytesleft);*/
	} else {
	    u2gb[j] = ((gb18030[0] << 8) & 0xff00) | (gb18030[1] & 0xff);
	    /*printf("U+%04x converts to GB %04x (%02x %02x)\n", j, u2gb[j],
		    gb18030[0] & 0xff, gb18030[1] & 0xff); */
	}
    }

    for (j = 0; j <= 0xffff; j += PER_ROW) {
	int k;
	int any = 0;

	for (k = j; k < j + PER_ROW; k++) {
	    if (u2gb[k] != 0) {
		any = 1;
		break;
	    }
	}
	if (!any) {
	    printf("/* %04x */ NULL,\n", j);
	    continue;
	}

	printf("/* %04x */ \"", j);
	for (k = j; k < j + PER_ROW; k++) {
	    printf("\\x%02x\\x%02x",
		    (u2gb[k] >> 8) & 0xff,
		    u2gb[k] & 0xff);
	}
	printf("\",\n");
    }

    return 0;
}
