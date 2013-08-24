/*
 * Copyright (c) 2000-2010, 2013 Paul Mattes.
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
 *	printer.c
 *		Printer session support
 */

#include "globals.h"

#if (defined(C3270) || defined(X3270_DISPLAY)) && defined(X3270_PRINTER) /*[*/
#if defined(X3270_DISPLAY) /*[*/
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#endif /*]*/
#if defined(_WIN32) /*[*/
#include <windows.h>
#include <shellapi.h>
#endif /*]*/
#if !defined(_WIN32) /*[*/
#include <sys/wait.h>
#endif /*]*/
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <fcntl.h>
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
#include "printerc.h"
#include "printc.h"
#include "savec.h"
#if defined(C3270) /*[*/
#include "screenc.h"
#endif /*]*/
#include "tablesc.h"
#include "telnetc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "w3miscc.h"

#if defined(_MSC_VER) /*[*/
#include "Msc/deprecated.h"
#endif /*]*/

#define PRINTER_BUF	1024

/* Statics */
#if !defined(_WIN32) /*[*/
static int      printer_pid = -1;
#else /*][*/
static HANDLE	printer_handle = NULL;
#endif /*]*/
static enum {
	P_NONE,		/* no printer session */
	P_RUNNING,	/* pr3287/wpr3287 process running */
	P_TERMINATING	/* pr3287/wpr3287 process termination requested */
} printer_state = P_NONE;
#if defined(X3270_DISPLAY) /*[*/
static Widget	lu_shell = (Widget)NULL;
#endif /*]*/
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

/* Globals */

/*
 * Printer initialization function.
 */
void
printer_init(void)
{
	/* Register interest in host connects and mode changes. */
	register_schange(ST_CONNECT, printer_host_connect);
	register_schange(ST_3270_MODE, printer_host_connect);
	register_schange(ST_EXITING, printer_exiting);
}

/*
 * Printer Start-up function
 * If 'lu' is non-NULL, then use the specific-LU form.
 * If not, use the assoc form.
 */
void
printer_start(const char *lu)
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
	char *proxy_cmd = CN;	/* -proxy <spec> */
#if defined(_WIN32) /*[*/
	char *pcp_res = CN;
	char *printercp = CN;	/* -printercp <n> */
	char *cp_cmdline;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
#else /*][*/
	int stdout_pipe[2];
	int stderr_pipe[2];
#endif /*]*/
	char *printer_opts;
	Boolean associated = False;
	Boolean success = True;

#if defined(X3270_DISPLAY) /*[*/
	/* Make sure the popups are initted. */
	printer_popup_init();
#endif /*]*/

	/* Can't start two. */
	if (printer_state == P_RUNNING) {
		popup_an_error("Printer is already running");
		return;
	}

	/* Gotta be in 3270 mode. */
	if (!IN_3270) {
		popup_an_error("Not in 3270 mode");
		return;
	}

	/* Select the command line to use. */
	if (lu == CN) {
		/* Associate with the current session. */
		associated = True;

		/* Gotta be in TN3270E mode. */
		if (!IN_TN3270E) {
			popup_an_error("Not in TN3270E mode");
			return;
		}

		/* Gotta be connected to an LU. */
		if (connected_lu == CN) {
			popup_an_error("Not connected to a specific LU");
			return;
		}
		lu = connected_lu;
		cmdlineName = ResAssocCommand;
	} else {
		/* Specific LU passed in. */
		cmdlineName = ResLuCommandLine;
	}

	trace_dsn("Starting %s%s printer session.\n", lu,
		associated? " associated": "");

	/*
	 * If the printer process was terminated, but has not yet exited,
	 * wait for it to exit here. This will reduce, but not completely
	 * eliminate, the race between the old session to the host being torn
	 * down and the new one being set up.
	 */
	if (printer_state == P_TERMINATING) {
#if !defined(_WIN32) /*[*/
	    	int status;
#else /*][*/
		DWORD exit_code;
#endif /*]*/

		trace_dsn("Waiting for old printer session to exit.\n");
#if !defined(_WIN32) /*[*/
		if (waitpid(printer_pid, &status, 0) < 0) {
			popup_an_errno(errno,
				"Printer process waitpid() failed");
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
			popup_an_error("GetExitCodeProcess() for printer "
				"session failed: %s",
				win32_strerror(GetLastError()));
			return;
		}
		if (exit_code == STILL_ACTIVE) {
			popup_an_error("Printer process failed to exit (Get)");
			return;
		}

		CloseHandle(printer_handle);
		printer_handle = NULL;

		if (exit_code != 0) {
			popup_an_error("Printer process exited with status "
				"0x%lx", (long)exit_code);
		}

		CloseHandle(printer_handle);
		printer_handle = NULL;
#endif /*]*/
		trace_dsn("Old printer session exited.\n");
		printer_state = P_NONE;
		st_changed(ST_PRINTER, False);
	}

	/* Fetch the command line and command resources. */
	cmdline = get_resource(cmdlineName);
	if (cmdline == CN) {
		popup_an_error("%s resource not defined", cmdlineName);
		return;
	}
#if !defined(_WIN32) /*[*/
	cmd = get_resource(ResPrinterCommand);
	if (cmd == CN) {
		popup_an_error(ResPrinterCommand " resource not defined");
		return;
	}
#else /*][*/
	printerName = get_resource(ResPrinterName);
#endif /*]*/

	/* Construct the charset option. */
	(void) snprintf(charset_cmd, sizeof(charset_cmd), "-charset %s",
		get_charset_name());

	/* Construct proxy option. */
	if (appres.proxy != CN) {
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
	while ((s = strstr(s, "%L%")) != CN) {
		cmd_len += strlen(lu) - 3;
		s += 3;
	}
	s = cmdline;
	while ((s = strstr(s, "%H%")) != CN) {
		cmd_len += strlen(qualified_host) - 3;
		s += 3;
	}
#if !defined(_WIN32) /*[*/
	s = cmdline;
	while ((s = strstr(s, "%C%")) != CN) {
		cmd_len += strlen(cmd) - 3;
		s += 3;
	}
#endif /*]*/
	s = cmdline;
	while ((s = strstr(s, "%R%")) != CN) {
		cmd_len += strlen(charset_cmd) - 3;
		s += 3;
	}
	s = cmdline;
	while ((s = strstr(s, "%P%")) != CN) {
		cmd_len += (proxy_cmd? strlen(proxy_cmd): 0) - 3;
		s += 3;
	}
#if defined(_WIN32) /*[*/
	s = cmdline;
	while ((s = strstr(s, "%I%")) != CN) {
		cmd_len += (printercp? strlen(printercp): 0) - 3;
		s += 3;
	}
#endif /*]*/
	s = cmdline;
	while ((s = strstr(s, "%O%")) != CN) {
		cmd_len += (printer_opts? strlen(printer_opts): 0) - 3;
		s += 3;
	}
	s = cmdline;
	while ((s = strstr(s, "%V%")) != CN) {
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
			    	if (proxy_cmd != CN)
					(void) strcat(cmd_text, proxy_cmd);
				s += 2;
				continue;
#if defined(_WIN32) /*[*/
			} else if (!strncmp(s+1, "I%", 2)) {
			    	if (printercp != CN)
					(void) strcat(cmd_text, printercp);
				s += 2;
				continue;
#endif /*]*/
			} else if (!strncmp(s+1, "O%", 2)) {
			    	if (printer_opts != CN)
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
			}
		}
		buf1[0] = c;
		buf1[1] = '\0';
		(void) strcat(cmd_text, buf1);
	}

#if !defined(_WIN32) /*[*/
	trace_dsn("Printer command: %s\n", cmd_text);

	/* Make pipes for printer's stdout and stderr. */
	if (pipe(stdout_pipe) < 0) {
		popup_an_errno(errno, "pipe() failed");
		Free(cmd_text);
		if (proxy_cmd != CN)
			Free(proxy_cmd);
		return;
	}
	(void) fcntl(stdout_pipe[0], F_SETFD, 1);
	if (pipe(stderr_pipe) < 0) {
		popup_an_errno(errno, "pipe() failed");
		(void) close(stdout_pipe[0]);
		(void) close(stdout_pipe[1]);
		Free(cmd_text);
		if (proxy_cmd != CN)
			Free(proxy_cmd);
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
		(void) execlp("/bin/sh", "sh", "-c", cmd_text, CN);
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

	trace_dsn("Printer command: %s\n", cp_cmdline);
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
	if (proxy_cmd != CN)
		Free(proxy_cmd);
#if defined(_WIN32) /*[*/
	if (printercp != CN)
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
#if defined(X3270_DISPLAY) /*[*/
		popup_printer_output(is_err, is_dead? NULL: printer_stop,
		    "%s", p->buf);
#else /*][*/
		action_output("%s", p->buf);
#endif
		p->count = 0;
	}
}
#endif /*]*/

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
	if (printer_state != P_TERMINATING) {
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

		if (printer_state != P_TERMINATING) {
			popup_an_error("Printer process exited with status "
				"0x%lx", (long)exit_code);
		}
	} else {
		/* It is still running. */
		return;
	}
#endif /*]*/

	/* Update and propagate the state. */
	trace_dsn("Printer session exited.\n");
	printer_state = P_NONE;
	st_changed(ST_PRINTER, False);
}

/* Close the printer session. */
void
printer_stop(void)
{
	if (printer_state != P_RUNNING) {
		return;
	}
	trace_dsn("Stopping printer session.\n");

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

	/* Kill the process. */
#if defined(_WIN32) /*[*/
	if (printer_handle != NULL) {
		TerminateProcess(printer_handle, 0);
		printer_state = P_TERMINATING;
	}
#else /*][*/
	if (printer_pid != -1) {
		(void) kill(-printer_pid, SIGTERM);
		printer_state = P_TERMINATING;
	}
#endif /*]*/
}

/* The emulator is exiting.  Make sure the printer session is cleaned up. */
static void
printer_exiting(Boolean b _is_unused)
{
	printer_stop();
}

#if defined(X3270_DISPLAY) /*[*/
/* Callback for "OK" button on printer specific-LU popup */
static void
lu_callback(Widget w, XtPointer client_data, XtPointer call_data _is_unused)
{
	char *lu;

	if (w) {
		lu = XawDialogGetValueString((Widget)client_data);
		if (lu == CN || *lu == '\0') {
			popup_an_error("Must supply an LU");
			return;
		} else
			XtPopdown(lu_shell);
	} else
		lu = (char *)client_data;
	printer_start(lu);
}
#endif /*]*/

/* Host connect/disconnect/3270-mode event. */
static void
printer_host_connect(Boolean connected _is_unused)
{
	if (IN_3270) {
		char *printer_lu = appres.printer_lu;

		if (printer_lu != CN && !printer_running()) {
			if (!strcmp(printer_lu, ".")) {
				if (IN_TN3270E) {
					/* Associate with TN3270E session. */
					printer_start(CN);
				}
			} else {
				/* Specific LU. */
				printer_start(printer_lu);
			}
		} else if (!IN_E &&
			   printer_lu != CN &&
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
	}
}

#if defined(X3270_DISPLAY) /*[*/
/* Pop up the LU dialog box. */
void
printer_lu_dialog(void)
{
	if (lu_shell == NULL)
		lu_shell = create_form_popup("printerLu",
		    lu_callback, (XtCallbackProc)NULL, FORM_NO_WHITE);
	popup_popup(lu_shell, XtGrabExclusive);
}
#endif /*]*/

Boolean
printer_running(void)
{
	return (printer_state == P_RUNNING);
}

#endif /*]*/
