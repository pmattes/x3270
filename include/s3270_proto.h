/*
 * Copyright (c) 2018-2024 Paul Mattes.
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
 *	s3270_proto.h
 *		The s3270 protocol.
 */

/* Environment variables passed to child scripts. */
#define OUTPUT_ENV	"X3270OUTPUT"
#define INPUT_ENV	"X3270INPUT"
#define PORT_ENV	"X3270PORT"
#define URL_ENV		"X3270URL"

/* Common length for all prefixes. */
#define PREFIX_LEN	6

/* Prefixes for data output. */
#define DATA_PREFIX	"data: "
#define ERROR_DATA_PREFIX "errd: "

/* Prefixes for input. */
#define INPUT_PREFIX	"inpt: "
#define PWINPUT_PREFIX	"inpw: "

/* Prompt terminators. */
#define PROMPT_OK	"ok"
#define PROMPT_ERROR	"error"

/* Action to continue or abort interactive input. */
#define RESUME_INPUT	"ResumeInput"
#define RESUME_INPUT_ABORT	"-Abort"

/* JSON object member names. */
#define JRET_RESULT	"result"
#define JRET_RESULT_ERR	"result-err"
#define JRET_SUCCESS	"success"
#define JRET_STATUS	"status"
