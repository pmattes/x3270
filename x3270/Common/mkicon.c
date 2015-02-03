#include <stdio.h>

/* Make an icon file. */
int
main(int argc, char *argv[])
{
    int c;
    int i = 0;

    printf("unsigned char favicon[] = {\n");
    while ((c = getchar()) != EOF) {
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
    return 0;
}
