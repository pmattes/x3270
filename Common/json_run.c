/*
 * Copyright (c) 2014-2024 Paul Mattes.
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
 *      json_run.c
 *              JSON run command parsing
 */

#include "globals.h"

#include <inttypes.h>

#include "b3270proto.h"
#include "json.h"
#include "task.h"
#include "trace.h"
#include "utils.h"

#include "json_run.h"

/**
 * Compare two fixed-size strings
 *
 * @param[in] key	Non-NULL-terminated key
 * @param[in] key_length Length of key
 * @param[in] match_key	NULL-terminated key to match
 *
 * @return True if they match
 */
bool
json_key_matches(const char *key, size_t key_length, const char *match_key)
{
    size_t sl = strlen(match_key);

    return key_length == sl && !strncmp(key, match_key, sl);
}

/**
 * Free a vector of commands.
 *
 * @param[in,out] cmds	Commands to free
 *
 * @return NULL
 */
cmd_t **
free_cmds(cmd_t **cmds)
{

    if (cmds != NULL) {
	int cmd_ix;
	cmd_t *c;

	for (cmd_ix = 0; (c = cmds[cmd_ix]) != NULL; cmd_ix++) {
	    int arg_ix;

	    Free((char *)c->action);
	    c->action = NULL;
	    for (arg_ix = 0; c->args[arg_ix] != NULL; arg_ix++) {
		Free((char *)c->args[arg_ix]);
	    }
	    Free((void *)c->args);
	    Free(c);
	}
	Free(cmds);
    }
    return NULL;
}

/**
 * Parse a JSON-formatted command.
 *
 * @param[in] json	Parsed JSON object
 * @param[out] cmd	Parsed action and arguments
 * @param[out] errmsg	Error message if parsing fails
 *
 * @return True for success
 */
static bool
hjson_parse_one(const json_t *json, cmd_t **cmd, char **errmsg)
{
    char *action = NULL;
    json_t *jaction = NULL;
    const json_t *member;
    const char *key;
    size_t key_length;
    unsigned i;
    const char *value;
    size_t len;
    char **args = NULL;

    *errmsg = NULL;

    /*
     * It needs to be an object with one or two fields: action (a string) and
     * optional args (an array of scalar types).
     */
    if (json_type(json) != JT_OBJECT) {
	*errmsg = NewString("Not an object");
	goto fail;
    }

    /* Find the action. */
    if (!json_object_member(json, AttrAction, NT, &jaction)) {
	*errmsg = NewString("Missing object member 'action'");
	goto fail;
    }
    if (json_type(jaction) != JT_STRING) {
	*errmsg = NewString("Invalid '" AttrAction "' type");
	goto fail;
    }
    value = json_string_value(jaction, &len);
    action = Asprintf("%.*s", (int)len, value);

    BEGIN_JSON_OBJECT_FOREACH(json, key, key_length, member) {
	if (json_key_matches(key, key_length, AttrAction)) {
	    continue;
	}
	if (json_key_matches(key, key_length, AttrArgs)) {
	    unsigned array_length;

	    if (json_type(member) != JT_ARRAY) {
		*errmsg = NewString("Invalid '" AttrArgs "' type");
		goto fail;
	    }
	    array_length = json_array_length(member);
	    for (i = 0; i < array_length; i++) {
		const json_t *arg = json_array_element(member, i);

		switch (json_type(arg)) {
		case JT_NULL:
		case JT_BOOLEAN:
		case JT_INTEGER:
		case JT_DOUBLE:
		case JT_STRING:
		    break;
		default:
		    *errmsg = NewString("Invalid '" AttrArgs "' element type");
		    goto fail;
		}
	    }
	    args = (char **)Calloc(array_length + 1, sizeof(char *));
	    for (i = 0; i < array_length; i++) {
		const json_t *arg = json_array_element(member, i);

		switch (json_type(arg)) {
		case JT_NULL:
		    args[i] = NewString("");
		    break;
		case JT_BOOLEAN:
		    args[i] = Asprintf("%s",
			    json_boolean_value(arg)? "true": "false");
		    break;
		case JT_INTEGER:
		    args[i] = Asprintf("%"PRId64, json_integer_value(arg));
		    break;
		case JT_DOUBLE:
		    args[i] = Asprintf("%g", json_double_value(arg));
		    break;
		case JT_STRING:
		    value = json_string_value(arg, &len);
		    args[i] = Asprintf("%.*s", (int)len, value);
		    break;
		default:
		    break;
		}
	    }
	    continue;
	}
	*errmsg = Asprintf("Unknown object member '%.*s'", (int)key_length,
		key);
	goto fail;
    } END_JSON_OBJECT_FOREACH(j, key, key_length, member);

    /* Default the arguments if necessary. */
    if (args == NULL) {
	args = (char **)Malloc(sizeof(char *));
	args[0] = NULL;
    }

    /* Return the command. */
    *cmd = (cmd_t *)Calloc(1, sizeof(cmd_t));
    (*cmd)->action = action;
    (*cmd)->args = (const char **)args;
    return true;

fail:
    if (action != NULL) {
	Replace(action, NULL);
    }
    if (args != NULL) {
	for (i = 0; args[i] != NULL; i++) {
	    Replace(args[i], NULL);
	}
	Replace(args, NULL);
    }
    return false;
}

/**
 * Parse a JSON-formatted command or a set of commands.
 *
 * @param[in] json	JSON object to split
 * @param[out] cmds	Parsed actions and arguments
 * @param[out] single	Parsed single action
 *l @param[out] errmsg	Error message if parsing fails
 *
 * @return True for success
 */
bool
hjson_split(const json_t *json, cmd_t ***cmds, char **single, char **errmsg)
{
    unsigned array_length;
    unsigned i;
    cmd_t **c = NULL;
    size_t len;

    *cmds = NULL;
    *single = NULL;
    *errmsg = NULL;

    /* The object can be a string, an object or an array of objects. */
    switch (json_type(json)) {
    case JT_STRING:
	*single = NewString(json_string_value(json, &len));
	return true;
    case JT_ARRAY:
	array_length = json_array_length(json);
	break;
    case JT_OBJECT:
	array_length = 1;
	break;
    default:
	*errmsg = NewString("Not a string, object or array");
	goto fail;
    }

    /* Allocate the vector. */
    c = (cmd_t **)Calloc(array_length + 1, sizeof(cmd_t *));

    switch (json_type(json)) {
    case JT_ARRAY:
	for (i = 0; i < array_length; i++) {
	    char *elt_error;

	    if (!hjson_parse_one(json_array_element(json, i), &c[i],
			&elt_error)) {
		*errmsg = Asprintf("Element %u: %s", i, elt_error);
		Free(elt_error);
		goto fail;
	    }
	}
	break;
    case JT_OBJECT:
	if (!hjson_parse_one(json, &c[0], errmsg)) {
	    goto fail;
	}
	break;
    default:
	break;
    }

    /* Success. */
    *cmds = c;
    return true;

fail:
    c = free_cmds(c);
    return false;
}

/**
 * Parse a JSON-formatted command or a set of commands.
 *
 * @param[in] cmd	Command to parse, in JSON format
 * @param[in] cmd_len	Length of command
 * @param[out] cmds	Parsed actions and arguments
 * @param[out] single	Parsed single action
 * @param[out] errmsg	Error message if parsing fails
 *
 * @return hjparse_ret_t
 */
hjparse_ret_t
hjson_parse(const char *cmd, size_t cmd_len, cmd_t ***cmds, char **single,
	char **errmsg)
{
    json_t *json;
    json_errcode_t errcode;
    json_parse_error_t *error;

    *cmds = NULL;
    *errmsg = NULL;

    /* Parse the JSON. */
    errcode = json_parse(cmd, cmd_len, &json, &error);
    if (errcode != JE_OK) {
	*errmsg = Asprintf("JSON parse error: line %d, column %d: %s",
		error->line, error->column, error->errmsg);
	json_free_both(json, error);
	return (errcode == JE_INCOMPLETE)? HJ_INCOMPLETE: HJ_BAD_SYNTAX;
    }

    if (!hjson_split(json, cmds, single, errmsg)) {
	json_free(json);
	return HJ_BAD_CONTENT;
    }

    json_free(json);
    return HJ_OK;
}
