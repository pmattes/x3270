/*
 * Copyright (c) 2012, Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Don Russell, Jeff Sparkes, GTRC
 *       nor their contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, DON RUSSELL, JEFF SPARKES AND
 * GTRC "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	ssl_dll.h
 *		External definitions for functions and data in ssl_dll.c, as
 *		well as #defined renames for the functions it defines (so
 *		they are not confused with the real ones).
 */

#if !defined(_WIN32) /*[*/
#error For Windows only
#endif /*]*/

int ssl_dll_init(void);
extern const char *ssl_fail_reason;

#define ERR_error_string my_ERR_error_string
#define ERR_get_error my_ERR_get_error
#define SSLv23_method my_SSLv23_method
#define SSL_connect my_SSL_connect
#define SSL_CTX_check_private_key my_SSL_CTX_check_private_key
#define SSL_CTX_ctrl my_SSL_CTX_ctrl
#define SSL_CTX_free my_SSL_CTX_free
#define SSL_CTX_load_verify_locations my_SSL_CTX_load_verify_locations
#define SSL_CTX_new my_SSL_CTX_new
#define SSL_CTX_set_default_passwd_cb my_SSL_CTX_set_default_passwd_cb
#define SSL_CTX_set_default_verify_paths my_SSL_CTX_set_default_verify_paths
#define SSL_CTX_set_info_callback my_SSL_CTX_set_info_callback
#define SSL_CTX_use_certificate_chain_file my_SSL_CTX_use_certificate_chain_file
#define SSL_CTX_use_certificate_file my_SSL_CTX_use_certificate_file
#define SSL_CTX_use_PrivateKey_file my_SSL_CTX_use_PrivateKey_file
#define SSL_free my_SSL_free
#define SSL_get_verify_result my_SSL_get_verify_result
#define SSL_library_init my_SSL_library_init
#define SSL_load_error_strings my_SSL_load_error_strings
#define SSL_new my_SSL_new
#define SSL_read my_SSL_read
#define SSL_set_fd my_SSL_set_fd
#define SSL_set_verify my_SSL_set_verify
#define SSL_set_verify_depth my_SSL_set_verify_depth
#define SSL_shutdown my_SSL_shutdown
#define SSL_state_string my_SSL_state_string
#define SSL_state_string_long my_SSL_state_string_long
#define SSL_write my_SSL_write
#define X509_STORE_CTX_get_error my_X509_STORE_CTX_get_error
#define X509_verify_cert_error_string my_X509_verify_cert_error_string
