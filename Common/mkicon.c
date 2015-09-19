#include <stdio.h>

/*
 * Make an icon file.
 *
 * mkicon icon-file >c-file
 */
int
main(int argc, char *argv[])
{
    FILE *f;
    int c;
    int i = 0;
    
    if (argc != 2) {
	fprintf(stderr, "Usage: %s icon-file >c-file\n", argv[0]);
	return 1;
    }


    f = fopen(argv[1], "rb");
    if (f == NULL) {
	perror(argv[1]);
	return 1;
    }

    printf("unsigned char favicon[] = {\n");
    while ((c = fgetc(f)) != EOF) {
	if (i && !(i % 16)) {
	    printf("\n");
	}
	printf("%3d,", c);
	i++;
    }
    if ((i % 16) != 1) {
	printf("\n");
    }
    printf("};\nunsigned favicon_size = sizeof(favicon);\n");

    fclose(f);
    return 0;
}
