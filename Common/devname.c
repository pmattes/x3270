/* Set up to iterate. */
/*
 * Copyright (c) 2024 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	devname.c
 *		RFC 4777 device name support.
 *
 */

#include "globals.h"

#include "devname.h"
#include "utils.h"

/* Initialize a devname instance. */
devname_t *
devname_init(const char *template)
{
    devname_t *d;
    size_t len;
    size_t sub_length = 0;
    unsigned long max = 1;

    if (template == NULL || !template[0]) {
	return NULL;
    }

    d = Calloc(sizeof(devname_t) + (len = strlen(template)) + 1, 1);
    d->template = (char *)(d + 1);
    strcpy(d->template, template);

    /* Count the trailing '=' characters. */
    while (len > 0 && template[--len] == '=') {
	sub_length++;
	max *= 10;
    }

    /* Set up to iterate. */
    d->sub_length = sub_length;
    d->max = max - 1;
    d->template_length = strlen(template);
    d->current = 0;
    return d;
}

/* Iterate a devname. */
const char *
devname_next(devname_t *d)
{

    if (d->current < d->max) {
	/* Increment. */
	sprintf(d->template + d->template_length - d->sub_length, "%0*lu", (int)d->sub_length, ++d->current);
    }

    return d->template;
}

/* Free a devname. */
devname_t *
devname_free(devname_t *d)
{
    Free(d);
    return NULL;
}
