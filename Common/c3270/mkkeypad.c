#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Construct keypad data structures from a set of descriptor files.
 *
 * The files are:
 *  keypad.labels
 *   literal text to be drawn for the keypad
 *  keypad.outline
 *   outlines for the keys, ACS encoded ('l' for upper left, etc.)
 *  keypad.map
 *   sensitivity map for the keypad (aaaa is field 'a', etc.)
 *  keypad.full
 *   not used by this program, but gives the overall plan
 *
 * The result is an array of structures:
 *  unsigned char literal;	text from keypad.labels
 *  unsigned char outline;	ACS-encoded outline text
 *  sens_t *sens;		sensitivity, or NULL
 *
 * A sens_t is a structure:
 *  unsigned char ul_x, ul_y;	upper left corner
 *  unsigned char lr_x, lr_y;	lower right corner
 *  unsigned char callback_name; 'a', 'b', etc.
 */

typedef struct sensmap {
    struct sensmap *next;
    unsigned char name;
    unsigned ul_x, ul_y, lr_x, lr_y;
    int index;
    char *callback;
} sensmap_t;

sensmap_t *sensmaps = NULL;
sensmap_t *last_sensmap = NULL;
int sensmap_count = 0;

char *incdir = NULL;

FILE *fopen_inc(const char *name)
{
    FILE *f;
    char *path;

    if ((f = fopen(name, "r")) != NULL) {
	return f;
    }
    if (incdir == NULL) {
	return NULL;
    }
    path = malloc(strlen(incdir) + 1 + strlen(name) + 1);
    sprintf(path, "%s/%s", incdir, name);
    f = fopen(path, "r");
    free(path);
    return f;
}

int
main(int argc, char *argv[])
{
    FILE *callbacks;
    FILE *labels;
    FILE *outline;
    FILE *map;
    int c, d;
    unsigned x;
    unsigned y;
    sensmap_t *s;
    char buf[128];
    int cbl = 0;

    if (argc > 1 && !strncmp(argv[1], "-I", 2)) {
	incdir = argv[1] + 2;
    }

    /* Open the files. */
    labels = fopen_inc("keypad.labels");
    if (labels == NULL) {
	perror("keypad.labels");
	exit(1);
    }
    outline = fopen_inc("keypad.outline");
    if (outline == NULL) {
	perror("keypad.outline");
	exit(1);
    }
    map = fopen_inc("keypad.map");
    if (map == NULL) {
	perror("keypad.map");
	exit(1);
    }
    callbacks = fopen_inc("keypad.callbacks");
    if (callbacks == NULL) {
	perror("keypad.callbacks");
	exit(1);
    }

    /* Read in the map file first. */
    x = 0;
    y = 0;
    while ((c = fgetc(map)) != EOF) {
	if (c == '\n') {
	    y++;
	    x = 0;
	    continue;
	}
	if (c == ' ') {
	    x++;
	    continue;
	}
	for (s = sensmaps; s != NULL; s = s->next) {
	    if (s->name == c)
		break;
	}
	if (s != NULL) {
	    /* Seen it before. */
	    s->lr_x = x;
	    s->lr_y = y;
	} else {
	    s = (sensmap_t *)malloc(sizeof(sensmap_t));
	    if (s == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	    }
	    memset(s, '\0', sizeof(sensmap_t));
	    s->name = c;
	    s->ul_x = s->lr_x = x;
	    s->ul_y = s->lr_y = y;
	    s->index = sensmap_count++;
	    s->callback = NULL;
	    s->next = NULL;
	    if (last_sensmap)
		last_sensmap->next = s;
	    else
		sensmaps = s;
	    last_sensmap = s;
	    last_sensmap = s;
	}
	x++;
    }
    fclose(map);

    /* Read in the callbacks. */
    while (fgets(buf, sizeof(buf), callbacks) != NULL) {
	char *t;
	char c;
	size_t sl;

	cbl++;
	sl = strlen(buf);
	if (sl > 0 && buf[sl - 1] == '\n')
	    buf[sl - 1] = '\0';
	c = buf[0];
	if (!isalnum((unsigned char)c)) {
	    fprintf(stderr, "keypad.callbacks:%d Invalid callback character.\n",
		    cbl);
	    exit(1);
	}
	t = &buf[1];
	while (*t && isspace((unsigned char)*t)) {
	    t++;
	}
	if (!*t || !isalnum((unsigned char)*t)) {
	    fprintf(stderr, "keypad.callbacks:%d Invalid callback string.\n",
		    cbl);
	    exit(1);
	}
#if defined(MKK_DEBUG) /*[*/
	fprintf(stderr, "line %d: name '%c', callback '%s'\n", cbl, c, t);
#endif /*]*/
	for (s = sensmaps; s != NULL; s = s->next) {
	    if (s->name == c) {
		if (s->callback != NULL) {
		    fprintf(stderr, "keypad.callbacks:%d Duplicate callback "
			    "for '%c' (%s, %s).\n", cbl, c,
			    s->callback, t);
		    exit(1);
		}
		s->callback = malloc(strlen(t) + 1);
		if (s->callback == NULL) {
		    fprintf(stderr, "Out of memory.\n");
		    exit(1);
		}
		strcpy(s->callback, t);
		break;
	    }
	}
	if (s == NULL) {
	    fprintf(stderr, "keypad.callbacks:%d: Callback '%c' for "
		    "nonexistent map.\n", cbl, c);
	    exit(1);
	}
    }
    fclose(callbacks);
    for (s = sensmaps; s != NULL; s = s->next) {
	if (s->callback == NULL) {
	    fprintf(stderr, "Map '%c' has no callback.\n", s->name);
	    exit(1);
	}
    }

    /* Dump out the sensmaps. */
    printf("sens_t sens[%u] = {\n", sensmap_count);
    for (s = sensmaps; s != NULL; s = s->next) {
	printf("  { %2u, %2u, %2u, %2u, \"%s\" },\n",
		s->ul_x, s->ul_y, s->lr_x, s->lr_y, s->callback);
    }
    printf("};\n");

    /*
     * Read in the label and outline files, and use them to dump out the
     * keypad_desc[].
     */
    labels = fopen_inc("keypad.labels");
    if (labels == NULL) {
	perror("keypad.labels");
	exit(1);
    }
    outline = fopen_inc("keypad.outline");
    if (outline == NULL) {
	perror("keypad.outline");
	exit(1);
    }

    printf("keypad_desc_t keypad_desc[%u][80] = {\n", y);
    printf("{ /* row 0 */\n");

    x = 0;
    y = 0;
    while ((c = fgetc(labels)) != EOF) {
	d = fgetc(outline);
	if (c == '\n') {
	    if (d != '\n') {
		fprintf(stderr, "labels and outline out of sync at line %d\n",
			y + 1);
		exit(1);
	    }
	    y++;
	    x = 0;
	    continue;
	}
	if (x == 0 && y != 0)
	    printf("},\n{ /* row %u */\n", y);
	for (s = sensmaps; s != NULL; s = s->next) {
	    if (x >= s->ul_x && y >= s->ul_y &&
		x <= s->lr_x && y <= s->lr_y) {
		printf("  { '%c', '%c', &sens[%u] },\n",
			c, d, s->index);
		break;
	    }
	}
	if (s == NULL) {
	    if (c == ' ' && d == ' ')
		printf("  {   0,   0, NULL },\n");
	    else
		printf("  { '%c', '%c', NULL },\n", c, d);
	}
	x++;
    }
    if ((d = fgetc(outline)) != EOF) {
	fprintf(stderr, "labels and outline out of sync at EOF\n");
	exit(1);
    }
    printf("} };\n");
    fclose(labels);
    fclose(outline);

    return 0;
}
