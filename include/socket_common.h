/*
 * Copyright (c) 1993-2026 Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	socket_common.h
 *		Common socket definitions.
 */

#if !defined(_SOCKET_COMMON_H) /*[*/
# define _SOCKET_COMMON_H	1

/* Common socket definitions. */
# if !defined(_WIN32) /*[*/
#  include <sys/errno.h>
typedef int socket_t;
#  define INVALID_SOCKET  	(-1)
#  define INET_ADDR_T		in_addr_t
#  define SOCK_CLOSE(s)  	close(s)
#  define socket_errno()	errno
#  define socket_strerror(n)	strerror(n)
#  define SOCK_IOCTL(s, f, v)	ioctl(s, f, v)
#  define IOCTL_T		int
#  define IS_EWOULDBLOCK(e)	((e) == EWOULDBLOCK || (e) == EAGAIN)
#  define SE_ECONNRESET		ECONNRESET
#  define SE_EINTR		EINTR
#  define SE_EAGAIN		EAGAIN
#  define SE_EPIPE		EPIPE
#  if defined(EINPROGRESS) /*[*/
#   define SE_EINPROGRESS        EINPROGRESS
#  endif /*]*/
# else /*][*/
typedef SOCKET socket_t;
#  define INET_ADDR_T		unsigned long
#  define SOCK_CLOSE(s)  	closesocket(s)
#  define socket_errno()	WSAGetLastError()
#  define socket_strerror(n) 	win32_strerror(n)
#  define SOCK_IOCTL(s, f, v)	ioctlsocket(s, f, (DWORD *)v)
#  define IOCTL_T		u_long
#  define IS_EWOULDBLOCK(e)	((e) == WSAEWOULDBLOCK || (e) == EAGAIN)
#  define SE_ECONNRESET		WSAECONNRESET
#  define SE_EINTR		WSAEINTR
#  define SE_EAGAIN		WSAEINPROGRESS
#  define SE_EPIPE		WSAECONNABORTED
#  define SE_EINPROGRESS	WSAEINPROGRESS
#  if !defined(WSA_FLAG_NO_HANDLE_INHERIT) /*[*/
#   define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#  endif /*]*/
# endif /*]*/

# if defined(SE_EINPROGRESS) /*[*/
#  define IS_EINPROGRESS(e)	((e) == SE_EINPROGRESS)
# else /*][*/
#  define IS_EINPROGRESS(e)	false
# endif /*]*/

#endif /*]*/
