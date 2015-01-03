/*
 * Copyright (c) 2000-2010, 2013-2015 Paul Mattes.
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
 *	pr3287_session.c
 *		3287 printer session support
 */

#include "globals.h"

#if defined(X3270_INTERACTIVE) /*[*/
#if !defined(_WIN32) /*[*/
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /*]*/
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <assert.h>
#include "3270ds.h"
#include "appres.h"
#include "objects.h"
#include "resources.h"
#include "ctlr.h"

#include "charsetc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "pr3287_session.h"
#include "print_screen.h"
#include "savec.h"
#if defined(C3270) /*[*/
#include "screenc.h"
#endif /*]*/
#include "tablesc.h"
#include "telnetc.h"
#include "trace.h"
#include "utilc.h"
#include "w3miscc.h"
#include "xioc.h"

#define PRINTER_BUF	1024

#if !defined(_WIN32) /*[*/
#define SOCK_CLOSE(s)	close(s)
#else /*][*/
#define SOCK_CLOSE(s)	closesocket(s)
#endif /*]*/

#define PRINTER_DELAY_MS	3000
#define PRINTER_KILL_MS		5000

/* Statics */
#if !defined(_WIN32) /*[*/
static int      printer_pid = -1;
#else /*][*/
static HANDLE	printer_handle = NULL;
#endif /*]*/
static enum {
	P_NONE,		/* no printer session */
	P_DELAY,	/* delay before (re)starting pr3287 */
	P_RUNNING,	/* pr3287 process running */
	P_SHUTDOWN,	/* pr3287 graceful shutdown requested */
	P_TERMINATING	/* pr3287 forcible termination requested */
} printer_state = P_NONE;
static socket_t	printer_ls = INVALID_SOCKET;	/* printer sync listening socket */
static ioid_t	printer_ls_id = NULL_IOID; /* input ID */
#if defined(_WIN32) /*[*/
static HANDLE	printer_ls_handle = NULL;
#endif /*]*/
static socket_t	printer_sync = INVALID_SOCKET;	/* printer sync socket */
static ioid_t	printer_sync_id = NULL_IOID; /* input ID */
#if defined(_WIN32) /*[*/
static HANDLE	printer_sync_handle = NULL;
#endif /*]*/
static ioid_t	printer_kill_id = NULL_IOID; /* kill timeout ID */
static ioid_t	printer_delay_id = NULL_IOID; /* delay timeout ID */
static char	*printer_delay_lu = NULL;
static Boolean	printer_delay_associated = False;
static struct pr3o {
	int fd;			/* file descriptor */
	ioid_t input_id;	/* input ID */
	ioid_t timeout_id; 	/* timeout ID */
	int count;		/* input count */
	char buf[PRINTER_BUF];	/* input buffer */
} printer_stdout = { -1, 0L, 0L, 0 },
  printer_stderr = { -1, 0L, 0L, 0 };

#if !defined(_WIN32) /*[*/
static void	printer_output(unsigned long fd, ioid_t id);
static void	printer_error(unsigned long fd, ioid_t id);
static void	printer_otimeout(ioid_t id);
static void	printer_etimeout(ioid_t id);
static void	printer_dump(struct pr3o *p, Boolean is_err, Boolean is_dead);
#endif /*]*/
static void	printer_host_connect(Boolean connected _is_unused);
static void	printer_exiting(Boolean b _is_unused);
static void	printer_accept(unsigned long fd, ioid_t id);
static void	printer_start_now(const char *lu, Boolean associated);

/* Globals */

/*
 * Printer initialization function.
 */
void
pr3287_session_init(void)
{
    /* Register interest in host connects and mode changes. */
    register_schange(ST_CONNECT, printer_host_connect);
    register_schange(ST_3270_MODE, printer_host_connect);
    register_schange(ST_EXITING, printer_exiting);
}

/*
 * If the printer process was terminated, but has not yet exited, wait for it
 * to exit.
 */
static void
printer_reap_now(void)
{
#if !defined(_WIN32) /*[*/
	int status;
#else /*][*/
	DWORD exit_code;
#endif /*]*/

	assert(printer_state == P_TERMINATING);

	vtrace("Waiting for old printer session to exit.\n");
#if !defined(_WIN32) /*[*/
	if (waitpid(printer_pid, &status, 0) < 0) {
		popup_an_errno(errno, "Printer process waitpid() failed");
		return;
	}
	--children;
	printer_pid = -1;
#else /*][*/
	if (WaitForSingleObject(printer_handle, 2000) == WAIT_TIMEOUT) {
		popup_an_error("Printer process failed to exit (Wait)");
		return;
	}
	if (GetExitCodeProcess(printer_handle, &exit_code) == 0) {
		popup_an_error("GetExitCodeProcess() for printer session "
			"failed: %s", win32_strerror(GetLastError()));
		return;
	}
	if (exit_code == STILL_ACTIVE) {
		popup_an_error("Printer process failed to exit (Get)");
		return;
	}

	CloseHandle(printer_handle);
	printer_handle = NULL;

	if (exit_code != 0) {
		popup_an_error("Printer process exited with status 0x%lx",
			(long)exit_code);
	}

	CloseHandle(printer_handle);
	printer_handle = NULL;
#endif /*]*/

	vtrace("Old printer session exited.\n");
	printer_state = P_NONE;
	st_changed(ST_PRINTER, False);
}

/* Delayed start function. */
static void
delayed_start(ioid_t id _is_unused)
{
	assert(printer_state == P_DELAY);

	vtrace("Printer session start delay complete.\n");

	/* Start the printer. */
	printer_state = P_NONE;
	assert(printer_delay_lu != NULL);
	printer_start_now(printer_delay_lu, printer_delay_associated);

	/* Forget the saved state. */
	printer_delay_id = NULL_IOID;
	Free(printer_delay_lu);
	printer_delay_lu = NULL;
}

/*
 * Printer start-up function.
 *
 * If 'lu' is non-NULL, then use the specific-LU form.
 * If not, use the assoc form.
 *
 * This function may just store the parameters and let a timeout start the
 * process. It can also be invoked interactively, and might fail.
 */
void
printer_start(const char *lu)
{
	Boolean associated = False;

	/* Gotta be in 3270 mode. */
	if (!IN_3270) {
		popup_an_error("Not in 3270 mode");
		return;
	}

	/* Figure out the LU. */
	if (lu == NULL) {
		/* Associate with the current session. */
		associated = True;

		/* Gotta be in TN3270E mode. */
		if (!IN_TN3270E) {
			popup_an_error("Not in TN3270E mode");
			return;
		}

		/* Gotta be connected to an LU. */
		if (connected_lu == NULL) {
			popup_an_error("Not connected to a specific LU");
			return;
		}
		lu = connected_lu;
	}

	/* Can't start two. */
	switch (printer_state) {
	case P_NONE:
	    	/*
		 * Remember what was requested, and set a timeout to start the
		 * new session.
		 */
		vtrace("Delaying printer session start %dms.\n",
			PRINTER_DELAY_MS);
		Replace(printer_delay_lu, NewString(lu));
		printer_delay_associated = associated;
		printer_state = P_DELAY;
		printer_delay_id = AddTimeOut(PRINTER_DELAY_MS, delayed_start);
		break;
	case P_DELAY:
	case P_RUNNING:
		/* Redundant start request. */
		popup_an_error("Printer is already started or running");
		return;
	case P_SHUTDOWN:
		/*
		 * Remember what was requested, and let the state change or
		 * timeout functions start the new session.
		 *
		 * There is a window here where two manual start commands could
		 * get in after a manual stop. This is needed because we can't
		 * distinguish a manual from an automatic start.
		 */
		vtrace("Delaying printer session start %dms after exit.\n",
			PRINTER_DELAY_MS);
		Replace(printer_delay_lu, NewString(lu));
		printer_delay_associated = associated;
		return;
	case P_TERMINATING:
		/* Collect the exit status now and start the new session. */
		printer_reap_now();
		printer_start_now(lu, associated);
		break;
	}
}

/*
 * Synchronous printer start-up function.
 *
 * Called when it is safe to start a pr3287 session.
 */
static void
printer_start_now(const char *lu, Boolean associated)
{
	const char *cmdlineName;
	const char *cmdline;
#if !defined(_WIN32) /*[*/
	const char *cmd;
#else /*][*/
	const char *printerName;
#endif /*]*/
	int cmd_len = 0;
	const char *s;
	char *cmd_text;
	char c;
	char charset_cmd[256];	/* -charset <csname> */
	char *proxy_cmd = NULL;	/* -proxy <spec> */
#if defined(_WIN32) /*[*/
	char *pcp_res = NULL;
	char *printercp = NULL;	/* -printercp <n> */
	char *cp_cmdline;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
#else /*][*/
	int stdout_pipe[2];
	int stderr_pipe[2];
#endif /*]*/
	char *printer_opts;
	Boolean success = True;
	struct sockaddr_in printer_lsa;
	socklen_t len;
	char syncopt[64];

	assert(printer_state == P_NONE);

	/* Select the command line to use. */
	if (associated) {
		cmdlineName = ResAssocCommand;
	} else {
		cmdlineName = ResLuCommandLine;
	}

	vtrace("Starting %s%s printer session.\n", lu,
		associated? " associated": "");

	/* Create a listening socket for pr3287 to connect back to. */
	printer_ls = socket(PF_INET, SOCK_STREAM, 0);
	if (printer_ls == INVALID_SOCKET) {
		popup_a_sockerr("socket(printer sync)");
		return;
	}
	memset(&printer_lsa, '\0', sizeof(printer_lsa));
	printer_lsa.sin_family = AF_INET;
	printer_lsa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(printer_ls, (struct sockaddr *)&printer_lsa,
		    sizeof(printer_lsa)) < 0) {
		popup_a_sockerr("bind(printer sync)");
		SOCK_CLOSE(printer_ls);
		return;
	}
	memset(&printer_lsa, '\0', sizeof(printer_lsa));
	printer_lsa.sin_family = AF_INET;
	len = sizeof(printer_lsa);
	if (getsockname(printer_ls, (struct sockaddr *)&printer_lsa,
		    &len) < 0) {
		popup_a_sockerr("getsockname(printer sync)");
		SOCK_CLOSE(printer_ls);
		return;
	}
	snprintf(syncopt, sizeof(syncopt), "%s %d",
		OptSyncPort, ntohs(printer_lsa.sin_port));
	if (listen(printer_ls, 5) < 0) {
		popup_a_sockerr("listen(printer sync)");
		SOCK_CLOSE(printer_ls);
		return;
	}
#if defined(_WIN32) /*[*/
	printer_ls_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (printer_ls_handle == NULL) {
		popup_an_error("CreateEvent: %s",
			win32_strerror(GetLastError()));
		SOCK_CLOSE(printer_ls);
		printer_ls = INVALID_SOCKET;
		return;
	}
	if (WSAEventSelect(printer_ls, printer_ls_handle, FD_ACCEPT) != 0) {
		popup_an_error("WSAEventSelect: %s",
			win32_strerror(GetLastError()));
		SOCK_CLOSE(printer_ls);
		printer_ls = INVALID_SOCKET;
		return;
	}
	printer_ls_id = AddInput((int)printer_ls_handle, printer_accept);
#else /*][*/
	printer_ls_id = AddInput(printer_ls, printer_accept);
#endif /*]*/

	/* Fetch the command line and command resources. */
	cmdline = get_resource(cmdlineName);
	if (cmdline == NULL) {
		popup_an_error("%s resource not defined", cmdlineName);
		SOCK_CLOSE(printer_ls);
		return;
	}
#if !defined(_WIN32) /*[*/
	cmd = get_resource(ResPrinterCommand);
	if (cmd == NULL) {
		popup_an_error(ResPrinterCommand " resource not defined");
		SOCK_CLOSE(printer_ls);
		return;
	}
#else /*][*/
	printerName = get_resource(ResPrinterName);
#endif /*]*/

	/* Construct the charset option. */
	(void) snprintf(charset_cmd, sizeof(charset_cmd), "-charset %s",
		get_charset_name());

	/* Construct proxy option. */
	if (appres.proxy != NULL) {
#if !defined(_WIN32) /*[*/
	    	proxy_cmd = xs_buffer("-proxy \"%s\"", appres.proxy);
#else /*][ */
		proxy_cmd = xs_buffer("-proxy %s", appres.proxy);
#endif /*]*/
	}

#if defined(_WIN32) /*[*/
	/* Get the codepage for the printer. */
	pcp_res = get_resource(ResPrinterCodepage);
	if (pcp_res)
	    	printercp = xs_buffer("-printercp %s", pcp_res);
#endif /*]*/

	/* Get printer options. */
#if defined(C3270) /*[*/
	printer_opts = appres.printer_opts;
#else /*][*/
	printer_opts = get_resource(ResPrinterOptions);
#endif /*]*/

	/* Construct the command line. */

	/* Figure out how long it will be. */
	cmd_len = strlen(cmdline) + 1;
	s = cmdline;
	while ((s = strstr(s, "%L%")) != NULL) {
		cmd_len += strlen(lu) - 3;
		s += 3;
	}
	s = cmdline;
	while ((s = strstr(s, "%H%")) != NULL) {
		cmd_len += strlen(qualified_host) - 3;
		s += 3;
	}
#if !defined(_WIN32) /*[*/
	s = cmdline;
	while ((s = strstr(s, "%C%")) != NULL) {
		cmd_len += strlen(cmd) - 3;
		s += 3;
	}
#endif /*]*/
	s = cmdline;
	while ((s = strstr(s, "%R%")) != NULL) {
		cmd_len += strlen(charset_cmd) - 3;
		s += 3;
	}
	s = cmdline;
	while ((s = strstr(s, "%P%")) != NULL) {
		cmd_len += (proxy_cmd? strlen(proxy_cmd): 0) - 3;
		s += 3;
	}
#if defined(_WIN32) /*[*/
	s = cmdline;
	while ((s = strstr(s, "%I%")) != NULL) {
		cmd_len += (printercp? strlen(printercp): 0) - 3;
		s += 3;
	}
#endif /*]*/
	s = cmdline;
	while ((s = strstr(s, "%O%")) != NULL) {
		cmd_len += (printer_opts? strlen(printer_opts): 0) - 3;
		s += 3;
	}
	s = cmdline;
	while ((s = strstr(s, "%V%")) != NULL) {
#if defined(HAVE_LIBSSL) /*[*/
		cmd_len += appres.verify_host_cert?
		    strlen(OptVerifyHostCert) + 1: 0;
		cmd_len += appres.self_signed_ok?
		    strlen(OptSelfSignedOk) + 1: 0;
		cmd_len += appres.ca_dir?
		    strlen(OptCaDir) + 4 + strlen(appres.ca_dir): 0;
		cmd_len += appres.ca_file?
		    strlen(OptCaFile) + 4 + strlen(appres.ca_file): 0;
		cmd_len += appres.cert_file?
		    strlen(OptCertFile) + 4 + strlen(appres.cert_file): 0;
		cmd_len += appres.cert_file_type?
		    strlen(OptCertFileType) + 2 + strlen(appres.cert_file_type):
		    0;
		cmd_len += appres.chain_file?
		    strlen(OptChainFile) + 4 + strlen(appres.chain_file): 0;
		cmd_len += appres.key_file?
		    strlen(OptChainFile) + 4 + strlen(appres.key_file): 0;
		/*
		 * XXX: I hope the key password has no double quotes. I could
		 * fix it on Unix, but not on Windows.
		 */
		cmd_len += appres.key_passwd?
		    strlen(OptKeyPasswd) + 4 + strlen(appres.key_passwd): 0;
		cmd_len += appres.accept_hostname?
		    strlen(OptAcceptHostname) + 4 +
		    strlen(appres.accept_hostname): 0;
#endif /*]*/
		cmd_len -= 3;
		s += 3;
	}
	s = cmdline;
	while ((s = strstr(s, "%S%")) != NULL) {
		cmd_len += strlen(syncopt) - 3;
		s += 3;
	}

	/* Allocate a string buffer and substitute into it. */
	cmd_text = Malloc(cmd_len);
	cmd_text[0] = '\0';
	for (s = cmdline; (c = *s) != '\0'; s++) {
		char buf1[2];

		if (c == '%') {
			if (!strncmp(s+1, "L%", 2)) {
				(void) strcat(cmd_text, lu);
				s += 2;
				continue;
			} else if (!strncmp(s+1, "H%", 2)) {
				(void) strcat(cmd_text, qualified_host);
				s += 2;
				continue;
#if !defined(_WIN32) /*[*/
			} else if (!strncmp(s+1, "C%", 2)) {
				(void) strcat(cmd_text, cmd);
				s += 2;
				continue;
#endif /*]*/
			} else if (!strncmp(s+1, "R%", 2)) {
				(void) strcat(cmd_text, charset_cmd);
				s += 2;
				continue;
			} else if (!strncmp(s+1, "P%", 2)) {
			    	if (proxy_cmd != NULL)
					(void) strcat(cmd_text, proxy_cmd);
				s += 2;
				continue;
#if defined(_WIN32) /*[*/
			} else if (!strncmp(s+1, "I%", 2)) {
			    	if (printercp != NULL)
					(void) strcat(cmd_text, printercp);
				s += 2;
				continue;
#endif /*]*/
			} else if (!strncmp(s+1, "O%", 2)) {
			    	if (printer_opts != NULL)
					(void) strcat(cmd_text, printer_opts);
				s += 2;
				continue;
			} else if (!strncmp(s+1, "V%", 2)) {
#if defined(HAVE_LIBSSL) /*[*/
				if (appres.verify_host_cert)
					(void) strcat(cmd_text,
						" " OptVerifyHostCert);
				if (appres.self_signed_ok)
					(void) strcat(cmd_text,
						" " OptSelfSignedOk);
				if (appres.ca_dir) {
					(void) strcat(cmd_text, " " OptCaDir);
					(void) strcat(cmd_text, " ");
					(void) sprintf(strchr(cmd_text, '\0'),
						"\"%s\"", appres.ca_dir);
				}
				if (appres.ca_file) {
					(void) strcat(cmd_text, " " OptCaFile);
					(void) strcat(cmd_text, " ");
					(void) sprintf(strchr(cmd_text, '\0'),
						"\"%s\"", appres.ca_file);
				}
				if (appres.cert_file) {
					(void) strcat(cmd_text,
						" " OptCertFile);
					(void) strcat(cmd_text, " ");
					(void) sprintf(strchr(cmd_text, '\0'),
						"\"%s\"", appres.cert_file);
				}
				if (appres.cert_file_type) {
					(void) strcat(cmd_text,
						" " OptCertFileType);
					(void) strcat(cmd_text, " ");
					(void) strcat(cmd_text,
						appres.cert_file_type);
				}
				if (appres.chain_file) {
					(void) strcat(cmd_text,
						" " OptChainFile);
					(void) strcat(cmd_text, " ");
					(void) sprintf(strchr(cmd_text, '\0'),
						"\"%s\"", appres.chain_file);
				}
				if (appres.key_file) {
					(void) strcat(cmd_text, " " OptKeyFile);
					(void) strcat(cmd_text, " ");
					(void) sprintf(strchr(cmd_text, '\0'),
						"\"%s\"", appres.key_file);
				}
				if (appres.key_passwd) {
					(void) strcat(cmd_text,
						" " OptKeyPasswd);
					(void) strcat(cmd_text, " ");
					(void) sprintf(strchr(cmd_text, '\0'),
						"\"%s\"", appres.key_passwd);
				}
				if (appres.accept_hostname) {
					(void) strcat(cmd_text,
						" " OptAcceptHostname);
					(void) strcat(cmd_text, " ");
					(void) sprintf(strchr(cmd_text, '\0'),
					     "\"%s\"", appres.accept_hostname);
				}
#endif /*]*/
				s += 2;
				continue;
			} else if (!strncmp(s+1, "S%", 2)) {
				strcat(cmd_text, syncopt);
				s += 2;
				continue;
			}
		}
		buf1[0] = c;
		buf1[1] = '\0';
		(void) strcat(cmd_text, buf1);
	}

#if !defined(_WIN32) /*[*/
	vtrace("Printer command: %s\n", cmd_text);

	/* Make pipes for printer's stdout and stderr. */
	if (pipe(stdout_pipe) < 0) {
		popup_an_errno(errno, "pipe() failed");
		Free(cmd_text);
		if (proxy_cmd != NULL)
			Free(proxy_cmd);
		SOCK_CLOSE(printer_ls);
		return;
	}
	(void) fcntl(stdout_pipe[0], F_SETFD, 1);
	if (pipe(stderr_pipe) < 0) {
		popup_an_errno(errno, "pipe() failed");
		(void) close(stdout_pipe[0]);
		(void) close(stdout_pipe[1]);
		Free(cmd_text);
		if (proxy_cmd != NULL)
			Free(proxy_cmd);
		SOCK_CLOSE(printer_ls);
		return;
	}
	(void) fcntl(stderr_pipe[0], F_SETFD, 1);

	/* Fork and exec the printer session. */
	switch (printer_pid = fork()) {
	    case 0:	/* child process */
		(void) dup2(stdout_pipe[1], 1);
		(void) close(stdout_pipe[1]);
		(void) dup2(stderr_pipe[1], 2);
		(void) close(stderr_pipe[1]);
		if (setsid() < 0) {
			perror("setsid");
			_exit(1);
		}
		(void) execlp("/bin/sh", "sh", "-c", cmd_text, NULL);
		(void) perror("exec(printer)");
		_exit(1);
	    default:	/* parent process */
		(void) close(stdout_pipe[1]);
		printer_stdout.fd = stdout_pipe[0];
		(void) close(stderr_pipe[1]);
		printer_stderr.fd = stderr_pipe[0];
		printer_stdout.input_id = AddInput(printer_stdout.fd,
		    printer_output);
		printer_stderr.input_id = AddInput(printer_stderr.fd,
		    printer_error);
		++children;
		break;
	    case -1:	/* error */
		popup_an_errno(errno, "fork()");
		(void) close(stdout_pipe[0]);
		(void) close(stdout_pipe[1]);
		(void) close(stderr_pipe[0]);
		(void) close(stderr_pipe[1]);
		success = False;
		break;
	}
#else /*][*/
	/* Pass the command via the environment. */
	if (printerName != NULL) {
		static char pn_buf[1024];

		(void) snprintf(pn_buf, sizeof(pn_buf), "PRINTER=%s",
			printerName);
		putenv(pn_buf);
	}

	/* Create the wpr3287 process. */
	if (!strncasecmp(cmd_text, "wpr3287.exe", 11))
		cp_cmdline = xs_buffer("%s%s", instdir, cmd_text);
	else
		cp_cmdline = NewString(cmd_text);

	vtrace("Printer command: %s\n", cp_cmdline);
	if (printerName != NULL) {
		vtrace("Printer (via %%PRINTER%%): %s\n", printerName);
	}
	memset(&si, '\0', sizeof(si));
	si.cb = sizeof(pi);
	memset(&pi, '\0', sizeof(pi));
	if (!CreateProcess(NULL, cp_cmdline, NULL, NULL, FALSE,
		    DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
		popup_an_error("CreateProcess() for printer session failed: %s",
			win32_strerror(GetLastError()));
		success = False;
	} else {
		printer_handle = pi.hProcess;
		CloseHandle(pi.hThread);
	}
	Free(cp_cmdline);
#endif /*]*/

	Free(cmd_text);
	if (proxy_cmd != NULL)
		Free(proxy_cmd);
#if defined(_WIN32) /*[*/
	if (printercp != NULL)
		Free(printercp);
#endif /*]*/

	/* Tell everyone else. */
	if (success) {
		printer_state = P_RUNNING;
		st_changed(ST_PRINTER, True);
	}
}

#if !defined(_WIN32) /*[*/
/* There's data from the printer session. */
static void
printer_data(struct pr3o *p, Boolean is_err)
{
	int space;
	int nr;
	static char exitmsg[] = "Printer session exited";

	/* Read whatever there is. */
	space = PRINTER_BUF - p->count - 1;
	nr = read(p->fd, p->buf + p->count, space);

	/* Handle read errors and end-of-file. */
	if (nr < 0) {
		popup_an_errno(errno, "printer session pipe input");
		printer_stop();
		return;
	}
	if (nr == 0) {
		if (printer_stderr.timeout_id != NULL_IOID) {
			/*
			 * Append a termination error message to whatever the
			 * printer process said, and pop it up.
			 */
			p = &printer_stderr;
			space = PRINTER_BUF - p->count - 1;
			if (p->count && *(p->buf + p->count - 1) != '\n') {
				*(p->buf + p->count) = '\n';
				p->count++;
				space--;
			}
			(void) strncpy(p->buf + p->count, exitmsg, space);
			p->count += strlen(exitmsg);
			if (p->count >= PRINTER_BUF)
				p->count = PRINTER_BUF - 1;
			printer_dump(p, True, True);
		} else {
			popup_an_error("%s", exitmsg);
		}
		printer_stop();
		return;
	}

	/* Add it to the buffer, and add a NULL. */
	p->count += nr;
	p->buf[p->count] = '\0';

	/*
	 * If there's no more room in the buffer, dump it now.  Otherwise,
	 * give it a second to generate more output.
	 */
	if (p->count >= PRINTER_BUF - 1) {
		printer_dump(p, is_err, False);
	} else if (p->timeout_id == NULL_IOID) {
		p->timeout_id = AddTimeOut(1000,
		    is_err? printer_etimeout: printer_otimeout);
	}
}

/* The printer process has some output for us. */
static void
printer_output(unsigned long fd _is_unused, ioid_t id _is_unused)
{
	printer_data(&printer_stdout, False);
}

/* The printer process has some error output for us. */
static void
printer_error(unsigned long fd _is_unused, ioid_t id _is_unused)
{
	printer_data(&printer_stderr, True);
}

/* Timeout from printer output or error output. */
static void
printer_timeout(struct pr3o *p, Boolean is_err)
{
	/* Forget the timeout ID. */
	p->timeout_id = NULL_IOID;

	/* Dump the output. */
	printer_dump(p, is_err, False);
}

/* Timeout from printer output. */
static void
printer_otimeout(ioid_t id _is_unused)
{
	printer_timeout(&printer_stdout, False);
}

/* Timeout from printer error output. */
static void
printer_etimeout(ioid_t id _is_unused)
{
	printer_timeout(&printer_stderr, True);
}

/* Dump pending printer process output. */
static void
printer_dump(struct pr3o *p, Boolean is_err, Boolean is_dead)
{
	if (p->count) {
		/*
		 * Strip any trailing newline, and make sure the buffer is
		 * NULL terminated.
		 */
		if (p->buf[p->count - 1] == '\n')
			p->buf[--(p->count)] = '\0';
		else if (p->buf[p->count])
			p->buf[p->count] = '\0';

		/* Dump it and clear the buffer. */
		popup_printer_output(is_err, is_dead? NULL: printer_stop,
		    "%s", p->buf);
		p->count = 0;
	}
}
#endif /*]*/

/* Shut off printer sync input. */
static void
printer_stop_sync(void)
{
	assert(printer_sync_id != NULL_IOID);
	RemoveInput(printer_sync_id);
	printer_sync_id = NULL_IOID;
#if defined(_WIN32) /*[*/
	assert(printer_sync_handle != NULL);
	CloseHandle(printer_sync_handle);
	printer_sync_handle = NULL;
#endif /*]*/
	SOCK_CLOSE(printer_sync);
	printer_sync = INVALID_SOCKET;
}

/* Input from pr3287 on the synchronization socket. */
static void
printer_sync_input(unsigned long fd _is_unused, ioid_t id _is_unused)
{
	vtrace("Input/EOF on printer sync socket.\n");
	assert(printer_state >= P_RUNNING);

	/*
	 * We don't do anything at this point, besides clean up the state
	 * associated with the sync socket.
	 *
	 * The pr3287 session is considered gone when (1) it closes the sync
	 * socket and (2) it exits.  The only change in behavior when the sync
	 * socket is closed is that when we want to stop pr3287, we just start
	 * the timeout to force-terminate it, instead of closing the sync
	 * socket first and letting it clean itself up.
	 */

	/* No more need for the sync socket. */
	printer_stop_sync();
}

/* Shut off the printer sync listening socket. */
static void
printer_stop_listening(void)
{
	assert(printer_ls_id != NULL_IOID);
	assert(printer_ls != INVALID_SOCKET);
#if defined(_WIN32) /*[*/
	assert(printer_ls_handle != NULL);
#endif /*]*/

	RemoveInput(printer_ls_id);
	printer_ls_id = NULL_IOID;
#if defined(_WIN32) /*[*/
	CloseHandle(printer_ls_handle);
	printer_ls_handle = NULL;
#endif /*]*/
	SOCK_CLOSE(printer_ls);
	printer_ls = INVALID_SOCKET;
}

/* Accept a synchronization connection from pr3287. */
static void
printer_accept(unsigned long fd _is_unused, ioid_t id)
{
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);

	/* Accept the connection. */
	assert(printer_state == P_RUNNING);
	printer_sync = accept(printer_ls, (struct sockaddr *)&sin, &len);
	if (printer_sync == INVALID_SOCKET) {
		popup_a_sockerr("accept(printer sync)");
	} else {
		vtrace("Accepted sync connection from printer.\n");

#if defined(_WIN32) /*[*/
		printer_sync_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (printer_sync_handle == NULL) {
			popup_an_error("CreateEvent failed");
			x3270_exit(1);
		}
		if (WSAEventSelect(printer_sync, printer_sync_handle,
			    FD_READ | FD_CLOSE) != 0) {
			popup_an_error("Can't set socket handle events\n");
			x3270_exit(1);
		}
		printer_sync_id = AddInput((int)printer_sync_handle,
			printer_sync_input);
#else /*][*/
		printer_sync_id = AddInput(printer_sync, printer_sync_input);
#endif /*]*/
	}

	/* No more need for the listening socket. */
	printer_stop_listening();
}

/*
 * Check for an exited printer session.
 *
 * On Unix, this function is supplied with a process ID and status for an
 * exited child process. If there is a printer process running and its process
 * ID matches, process the rest of the state change.
 *
 * On Windows, this function is responsible for collecting the status of an
 * exited printer process, if any.
 */
void
printer_check(
#if !defined(_WIN32) /*[*/
	      pid_t pid, int status
#else /*][*/
	      void
#endif /*]*/
	                           )
{
#if defined(_WIN32) /*[*/
	DWORD exit_code;
#endif /*]*/

	if (printer_state == P_NONE) {
	    	return;
	}

#if !defined(_WIN32) /*[*/
	if (pid != printer_pid) {
	    	return;
	}

	/*
	 * If we didn't stop it on purpose, decode and display the printer's
	 * exit status.
	 */
	if (printer_state == P_RUNNING) {
		if (WIFEXITED(status)) {
			popup_an_error("Printer process exited with status %d",
				WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			popup_an_error("Printer process killed by signal %d",
				WTERMSIG(status));
		} else {
			popup_an_error("Printer process stopped by unknown "
				"status %d", status);
		}
	}
	printer_pid = -1;
#else /*][*/

	if (printer_handle != NULL &&
	    GetExitCodeProcess(printer_handle, &exit_code) != 0 &&
	    exit_code != STILL_ACTIVE) {

		CloseHandle(printer_handle);
		printer_handle = NULL;

		if (printer_state == P_RUNNING) {
			popup_an_error("Printer process exited with status "
				"0x%lx", (long)exit_code);
		}
	} else {
		/* It is still running. */
		return;
	}
#endif /*]*/

	/*
	 * Stop the pending printer kill request.
	 */
	if (printer_state == P_SHUTDOWN) {
		assert(printer_kill_id != NULL_IOID);
		RemoveTimeOut(printer_kill_id);
		printer_kill_id = NULL_IOID;
	}

	/* Update and propagate the state. */
	vtrace("Printer session exited.\n");
	if (printer_sync_id != NULL_IOID) {
		printer_stop_sync();
	}
	printer_state = P_NONE;
	st_changed(ST_PRINTER, False);

	/*
	 * If there is a pending request to start the printer, set a timeout to
	 * start it.
	 */
	if (printer_delay_lu != NULL) {
		printer_state = P_DELAY;
		printer_delay_id = AddTimeOut(PRINTER_DELAY_MS, delayed_start);
	}
}

/* Terminate pr3287, with prejudice. */
static void
printer_kill(ioid_t id _is_unused)
{
	vtrace("Forcibly terminating printer session.\n");

	/* Kill the process. */
#if defined(_WIN32) /*[*/
	assert(printer_handle != NULL);
	TerminateProcess(printer_handle, 0);
#else /*][*/
	assert(printer_pid != -1);
	(void) kill(-printer_pid, SIGTERM);
#endif /*]*/

	printer_kill_id = NULL_IOID;
	printer_state = P_TERMINATING;
}

/* Close the printer session. */
void
printer_stop()
{
	switch (printer_state) {
	case P_DELAY:
		vtrace("Canceling delayed printer session start.\n");
		assert(printer_delay_id != NULL_IOID);
		RemoveTimeOut(printer_delay_id);
		printer_delay_id = NULL_IOID;
		assert(printer_delay_lu != NULL);
		Free(printer_delay_lu);
		printer_delay_lu = NULL;
		break;
	case P_RUNNING:
		/* Run through the logic below. */
		break;
	default:
		/* Nothing interesting to do. */
		return;
	}

	vtrace("Stopping printer session.\n");

	/* Remove inputs. */
	if (printer_stdout.input_id) {
		RemoveInput(printer_stdout.input_id);
		printer_stdout.input_id = 0L;
	}
	if (printer_stderr.input_id) {
		RemoveInput(printer_stderr.input_id);
		printer_stderr.input_id = 0L;
	}

	/* Cancel timeouts. */
	if (printer_stdout.timeout_id != NULL_IOID) {
		RemoveTimeOut(printer_stdout.timeout_id);
		printer_stdout.timeout_id = NULL_IOID;
	}
	if (printer_stderr.timeout_id != NULL_IOID) {
		RemoveTimeOut(printer_stderr.timeout_id);
		printer_stderr.timeout_id = NULL_IOID;
	}

	/* Clear buffers. */
	printer_stdout.count = 0;
	printer_stderr.count = 0;

	/*
	 * If we have a sync socket connection, shut it down to signal pr3287
	 * to exit gracefully.
	 *
	 * Then set a timeout to terminate it not so gracefully.
	 */
	if (printer_sync != INVALID_SOCKET) {
		vtrace("Stopping printer by shutting down sync socket.\n");
		assert(printer_ls == INVALID_SOCKET);

		/* The separate shutdown() call is likely redundant. */
#if !defined(_WIN32) /*[*/
		shutdown(printer_sync, SHUT_WR);
#else /*][*/
		shutdown(printer_sync, SD_SEND);
#endif /*]*/

		/* We no longer care about printer sync input. */
		printer_stop_sync();
	} else {
		/*
		 * No sync socket. Too late to get one.
		 */
		vtrace("No sync socket.\n");
		printer_stop_listening();
	}

	printer_state = P_SHUTDOWN;
	printer_kill_id = AddTimeOut(PRINTER_KILL_MS, printer_kill);
}

/* The emulator is exiting.  Make sure the printer session is cleaned up. */
static void
printer_exiting(Boolean b _is_unused)
{
	if (printer_state >= P_RUNNING && printer_state < P_TERMINATING) {
		printer_kill(NULL_IOID);
	}
}

/* Host connect/disconnect/3270-mode event. */
static void
printer_host_connect(Boolean connected _is_unused)
{
	if (IN_3270) {
		char *printer_lu = appres.printer_lu;

		if (printer_lu != NULL && !printer_running()) {
			if (!strcmp(printer_lu, ".")) {
				if (IN_TN3270E) {
					/* Associate with TN3270E session. */
					printer_start(NULL);
				}
			} else {
				/* Specific LU. */
				printer_start(printer_lu);
			}
		} else if (!IN_E &&
			   printer_lu != NULL &&
			   !strcmp(printer_lu, ".") &&
			   printer_running()) {

			/* Stop an automatic associated printer. */
			printer_stop();
		}
	} else if (printer_running()) {
		/*
		 * We're no longer in 3270 mode, then we can no longer have a
		 * printer session.  This may cause some fireworks if there is
		 * a print job pending when we do this, so some sort of awful
		 * timeout may be needed.
		 */
		printer_stop();
	} else {
	    	/*
		 * Forget state associated with printer start-up.
		 */
		if (printer_state == P_DELAY) {
			printer_state = P_NONE;
		}
		if (printer_delay_id != NULL_IOID) {
			RemoveTimeOut(printer_delay_id);
			printer_delay_id = NULL_IOID;
		}
		if (printer_delay_lu != NULL) {
			Free(printer_delay_lu);
			printer_delay_lu = NULL;
		}
	}
}

Boolean
printer_running(void)
{
	return (printer_state == P_RUNNING);
}

#endif /*]*/
