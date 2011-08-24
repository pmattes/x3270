/*
 * Make a translation table.
 *
 * mktbl <name> <count> [<sorted-file>]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *me;

static void
usage(void)
{
	fprintf(stderr, "usage: %s <name> [<file>]\n", me);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *slash;
	char *name;
	FILE *f;
	char linebuf[256];
	unsigned short u, mb;
	int line = 0;
	unsigned short u_start = 0, mb_start = 0, length = 0;
	int count = 0;

	/* Identify yourself. */
	if ((slash = strrchr(argv[0], '/')) != NULL)
		me = slash + 1;
	else
		me = argv[0];

	/* Check syntax. */
	if (argc < 2 || argc > 3)
		usage();

	/* Open the file. */
	if (argc == 3) {
		f = fopen(argv[2], "r");
		if (f == NULL) {
			perror(argv[2]);
			exit(1);
		}
	} else {
		f = stdin;
	}

	/* Start talking. */
	name = argv[1];
	printf("static unsigned short %s_e[] = {", name);

	/* Read the table. */
	while (fgets(linebuf, sizeof(linebuf), f) != NULL) {
		line++;
		if (sscanf(linebuf, "%hx %hx", &u, &mb) != 2) {
			fprintf(stderr, "line %d: parse error\n", line);
			exit(1);
		}
		if (line == 1) {
			u_start = u;
			mb_start = mb;
			length = 1;
		} else {
			if (u == u_start + length &&
			    mb == mb_start + length)
				length++;
			else {
				printf("%s%s0x%04x, 0x%04x, %5u",
					count? ", ": "",
					(count % 3)? "": "\n ",
					u_start, mb_start, length);
				u_start = u;
				mb_start = mb;
				length = 1;
				count++;
			}
		}
	}
	printf("%s%s0x%04x, 0x%04x, %5u",
		count? ", ": "",
		(count % 3)? "": "\n ",
		u_start, mb_start, length);

	/* Clean up. */
	printf("\n};\n\n\
xl_t %s = {\n\
    XL_SIZE(%s_e),\n\
    %s_e\n\
};\n",
	    name, name, name);

	/* Close files and exit. */
	if (argc == 3)
		fclose(f);
	return 0;
}
