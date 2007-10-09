/*
 * Copyright 2007 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * wc3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	catf.c
 *		A Windows console-based 3270 Terminal Emulator
 *		A subset of the Unix 'tail -f' command.
 */

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

static int catf(char *filename);

int
main(int argc, char *argv[])
{
    	int rv;

    	if (argc != 2) {
	    	fprintf(stderr, "usage: catf <filename>\n");
		exit(1);
	}

	do {
	    	rv = catf(argv[1]);
	} while (rv == 0);

	exit(1);
}

/*
 * Tail the file.
 * Returns -1 for error, 0 for retry (file shrank or possibly disappeared).
 */
static int
catf(char *filename)
{
    	int fd;
	struct stat buf;
	off_t size;
	off_t fp = 0;
	char rbuf[16384];

	fd = open(filename, O_RDONLY | O_BINARY);
	if (fd < 0) {
	    	perror(filename);
		return -1;
	}

	if (fstat(fd, &buf) < 0) {
	    	perror(filename);
		return -1;
	}

	size = buf.st_size;

	for (;;) {
	    	while (fp < size) {
		    	int n2r, nr;

		    	if (size - fp > sizeof(rbuf))
			    	n2r = sizeof(rbuf);
			else
			    	n2r = size - fp;
			nr = read(fd, rbuf, n2r);
			if (nr < 0) {
			    	perror(filename);
				close(fd);
				return 0;
			}
			if (nr == 0) {
				printf("\nUNEXPECTED EOF\n");
			    	close(fd);
				return 0;
			}
			(void) write(1, rbuf, nr);
			fp += nr;
		}
		do {
			if (fstat(fd, &buf) < 0) {
			    	perror(filename);
				return -1;
			}
			if (buf.st_size < size) {
			    	printf("\ncatf: '%s' shrank -- reopening\n",
					filename);
			    	close(fd);
				return 0;
			}
			if (buf.st_size == size)
				Sleep(1 * 1000);
		} while (buf.st_size == size);
		size = buf.st_size;
	}
}
