/*
 * Copyright (c) 1993-2024 Paul Mattes.
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
 *      s3common.c
 *              Common logic fot the s3270 protocol.
 */

#include "globals.h"

#include <assert.h>

#include "json.h"
#include "s3270_proto.h"
#include "s3common.h"
#include "task.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"

/**
 * Initialize a JSON return object.
 *
 * @returns initialized object.
 */
json_t *
s3json_init(void)
{
    json_t *j = json_object();

    json_object_set(j, JRET_RESULT, NT, json_array());
    json_object_set(j, JRET_RESULT_ERR, NT, json_array());
    return j;
}

/**
 * Handle action output.
 *
 * @param[in] buf	Buffer
 * @param[in] len	Buffer length
 * @param[in] success	True if data, false if error message
 * @param[in] capabilities Capability flags
 * @param[in,out] json	JSON context
 * @param[out] raw	Raw output, if requested
 * @param[out] cooked	Cooked output, if requested and output is in s3270 mode.
 */
void
s3data(const char *buf, size_t len, bool success, unsigned capabilities, json_t *json, char **raw, char **cooked)
{
    /*
     * Enforce the implicit assumption that there are no newlines in the
     * output.
     */
    char *b;
    char *bnext;
    char *newline;

    /* Remove trailing newlines. */
    while (len > 0 && buf[len - 1] == '\n') {
	len--;
    }

    b = Asprintf("%.*s", (int)len, buf);
    bnext = b;

    if (json != NULL) {
	json_t *result_array;
	json_t *err_array;

	assert(json_object_member(json, JRET_RESULT, NT, &result_array));
	assert(json_object_member(json, JRET_RESULT_ERR, NT, &err_array));
	while ((newline = strchr(bnext, '\n')) != NULL) {
	    json_array_append(result_array, json_string(bnext, newline - bnext));
	    json_array_append(err_array, json_boolean(!success));
	    bnext = newline + 1;
	}
	json_array_append(result_array, json_string(bnext, strlen(bnext)));
	json_array_append(err_array, json_boolean(!success));
	if (raw != NULL) {
	    *raw = NULL;
	}
	if (cooked != NULL) {
	    *cooked = NULL;
	}
	Free(b);
    } else {
	if (cooked != NULL) {
	    varbuf_t r;

	    vb_init(&r);
	    while ((newline = strchr(bnext, '\n')) != NULL) {
		vb_appendf(&r, "%s%.*s\n", !success && (capabilities & CBF_ERRD)? ERROR_DATA_PREFIX: DATA_PREFIX,
			(int)(newline - bnext), bnext);
		bnext = newline + 1;
	    }
	    vb_appendf(&r, "%s%s\n", !success && (capabilities & CBF_ERRD)? ERROR_DATA_PREFIX: DATA_PREFIX, bnext);

	    *cooked = vb_consume(&r);
	}
	if (raw != NULL) {
	    *raw = b;
	} else {
	    Free(b);
	}
    }
}

/**
 * Callback for completion of one command executed from stdin.
 *
 * @param[in] handle		Callback handle
 * @param[in] success		True if child succeeded
 * @param[in,out] json		JSON context
 * @param[out] out		Output
 */
void
s3done(void *handle, bool success, json_t **json, char **out)
{
    char *prompt = task_cb_prompt(handle);

    vtrace("Output for %s: %s/%s\n", task_cb_name(handle), prompt, success? PROMPT_OK: PROMPT_ERROR);

    /* Print the prompt. */
    if (*json != NULL) {
	char *w;

	json_object_set(*json, JRET_SUCCESS, NT, json_boolean(success));
	json_object_set(*json, JRET_STATUS, NT, json_string(prompt, NT));
	*out = Asprintf("%s\n", w = json_write_o(*json, JW_ONE_LINE));
	json_free(*json);
	Free(w);
    } else {
	*out = Asprintf("%s\n%s\n", prompt, success? PROMPT_OK: PROMPT_ERROR);
    }
}
