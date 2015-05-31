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

#if !defined(_WIN32) /*[*/
# include <sys/wait.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif /*]*/
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <assert.h>
#include "appres.h"
#include "resources.h"

#include "charset.h"
#include "host.h"
#include "lazya.h"
#include "popups.h"
#include "pr3287_session.h"
#include "telnet_core.h"
#include "trace.h"
#include "utils.h"
#include "varbuf.h"
#include "w3misc.h"
#include "xio.h"

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
static int      pr3287_pid = -1;
#else /*][*/
static HANDLE	pr3287_handle = NULL;
#endif /*]*/
static enum {
    P_NONE,		/* no printer session */
    P_DELAY,		/* delay before (re)starting pr3287 */
    P_RUNNING,		/* pr3287 process running */
    P_SHUTDOWN,		/* pr3287 graceful shutdown requested */
    P_TERMINATING	/* pr3287 forcible termination requested */
} pr3287_state = P_NONE;
static socket_t	pr3287_ls = INVALID_SOCKET;	/* printer sync listening socket */
static ioid_t	pr3287_ls_id = NULL_IOID; /* input ID */
#if defined(_WIN32) /*[*/
static HANDLE	pr3287_ls_handle = NULL;
#endif /*]*/
static socket_t	pr3287_sync = INVALID_SOCKET;	/* printer sync socket */
static ioid_t	pr3287_sync_id = NULL_IOID; /* input ID */
#if defined(_WIN32) /*[*/
static HANDLE	pr3287_sync_handle = NULL;
#endif /*]*/
static ioid_t	pr3287_kill_id = NULL_IOID; /* kill timeout ID */
static ioid_t	pr3287_delay_id = NULL_IOID; /* delay timeout ID */
static char	*pr3287_delay_lu = NULL;
static bool	pr3287_delay_associated = false;
static struct pr3o {
    int fd;			/* file descriptor */
    ioid_t input_id;		/* input ID */
    ioid_t timeout_id; 		/* timeout ID */
    int count;			/* input count */
    char buf[PRINTER_BUF];	/* input buffer */
} pr3287_stdout = { -1, 0L, 0L, 0 },
  pr3287_stderr = { -1, 0L, 0L, 0 };

#if !defined(_WIN32) /*[*/
static void	pr3287_output(iosrc_t fd, ioid_t id);
static void	pr3287_error(iosrc_t fd, ioid_t id);
static void	pr3287_otimeout(ioid_t id);
static void	pr3287_etimeout(ioid_t id);
static void	pr3287_dump(struct pr3o *p, bool is_err, bool is_dead);
#endif /*]*/
static void	pr3287_host_connect(bool connected _is_unused);
static void	pr3287_exiting(bool b _is_unused);
static void	pr3287_accept(iosrc_t fd, ioid_t id);
static void	pr3287_start_now(const char *lu, bool associated);

/* Globals */

/**
 * Printer session module registration.
 */
void
pr3287_session_register(void)
{
    /* Register interest in host connects and mode changes. */
    register_schange(ST_CONNECT, pr3287_host_connect);
    register_schange(ST_3270_MODE, pr3287_host_connect);
    register_schange(ST_EXITING, pr3287_exiting);
}

/*
 * If the printer process was terminated, but has not yet exited, wait for it
 * to exit.
 */
static void
pr3287_reap_now(void)
{
#if !defined(_WIN32) /*[*/
    int status;
#else /*][*/
    DWORD exit_code;
#endif /*]*/

    assert(pr3287_state == P_TERMINATING);

    vtrace("Waiting for old printer session to exit.\n");
#if !defined(_WIN32) /*[*/
    if (waitpid(pr3287_pid, &status, 0) < 0) {
	popup_an_errno(errno, "Printer process waitpid() failed");
	return;
    }
    --children;
    pr3287_pid = -1;
#else /*][*/
    if (WaitForSingleObject(pr3287_handle, 2000) == WAIT_TIMEOUT) {
	popup_an_error("Printer process failed to exit (Wait)");
	return;
    }
    if (GetExitCodeProcess(pr3287_handle, &exit_code) == 0) {
	popup_an_error("GetExitCodeProcess() for printer session failed: %s",
		win32_strerror(GetLastError()));
	return;
    }
    if (exit_code == STILL_ACTIVE) {
	popup_an_error("Printer process failed to exit (Get)");
	return;
    }

    CloseHandle(pr3287_handle);
    pr3287_handle = NULL;

    if (exit_code != 0) {
	popup_an_error("Printer process exited with status 0x%lx",
		(long)exit_code);
    }

    CloseHandle(pr3287_handle);
    pr3287_handle = NULL;
#endif /*]*/

    vtrace("Old printer session exited.\n");
    pr3287_state = P_NONE;
    st_changed(ST_PRINTER, false);
}

/* Delayed start function. */
static void
delayed_start(ioid_t id _is_unused)
{
    assert(pr3287_state == P_DELAY);

    vtrace("Printer session start delay complete.\n");

    /* Start the printer. */
    pr3287_state = P_NONE;
    assert(pr3287_delay_lu != NULL);
    pr3287_start_now(pr3287_delay_lu, pr3287_delay_associated);

    /* Forget the saved state. */
    pr3287_delay_id = NULL_IOID;
    Free(pr3287_delay_lu);
    pr3287_delay_lu = NULL;
}

/*
 * Printer session start-up function.
 *
 * If 'lu' is non-NULL, then use the specific-LU form.
 * If not, use the assoc form.
 *
 * This function may just store the parameters and let a timeout start the
 * process. It can also be invoked interactively, and might fail.
 */
void
pr3287_session_start(const char *lu)
{
    bool associated = false;

    /* Gotta be in 3270 mode. */
    if (!IN_3270) {
	popup_an_error("Not in 3270 mode");
	return;
    }

    /* Figure out the LU. */
    if (lu == NULL) {
	/* Associate with the current session. */
	associated = true;

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
    switch (pr3287_state) {
    case P_NONE:
	/*
	 * Remember what was requested, and set a timeout to start the
	 * new session.
	 */
	vtrace("Delaying printer session start %dms.\n", PRINTER_DELAY_MS);
	Replace(pr3287_delay_lu, NewString(lu));
	pr3287_delay_associated = associated;
	pr3287_state = P_DELAY;
	pr3287_delay_id = AddTimeOut(PRINTER_DELAY_MS, delayed_start);
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
	Replace(pr3287_delay_lu, NewString(lu));
	pr3287_delay_associated = associated;
	return;
    case P_TERMINATING:
	/* Collect the exit status now and start the new session. */
	pr3287_reap_now();
	pr3287_start_now(lu, associated);
	break;
    }
}

/*
 * Synchronous printer start-up function.
 *
 * Called when it is safe to start a pr3287 session.
 */
static void
pr3287_start_now(const char *lu, bool associated)
{
    const char *cmdlineName;
    const char *cmdline;
#if !defined(_WIN32) /*[*/
    const char *cmd;
#else /*][*/
    const char *printerName;
#endif /*]*/
    const char *s;
    char *cmd_text;
    char c;
    char *charset_cmd;		/* -charset <csname> */
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
    char *pr3287_opts;
    bool success = true;
    struct sockaddr_in pr3287_lsa;
    socklen_t len;
    char *syncopt;
    varbuf_t r;

    assert(pr3287_state == P_NONE);

    /* Select the command line to use. */
    if (associated) {
	cmdlineName = ResAssocCommand;
    } else {
	cmdlineName = ResLuCommandLine;
    }

    vtrace("Starting %s%s printer session.\n", lu,
	    associated? " associated": "");

    /* Create a listening socket for pr3287 to connect back to. */
    pr3287_ls = socket(PF_INET, SOCK_STREAM, 0);
    if (pr3287_ls == INVALID_SOCKET) {
	popup_a_sockerr("socket(printer sync)");
	return;
    }
    memset(&pr3287_lsa, '\0', sizeof(pr3287_lsa));
    pr3287_lsa.sin_family = AF_INET;
    pr3287_lsa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(pr3287_ls, (struct sockaddr *)&pr3287_lsa,
		sizeof(pr3287_lsa)) < 0) {
	popup_a_sockerr("bind(printer sync)");
	SOCK_CLOSE(pr3287_ls);
	return;
    }
    memset(&pr3287_lsa, '\0', sizeof(pr3287_lsa));
    pr3287_lsa.sin_family = AF_INET;
    len = sizeof(pr3287_lsa);
    if (getsockname(pr3287_ls, (struct sockaddr *)&pr3287_lsa, &len) < 0) {
	popup_a_sockerr("getsockname(printer sync)");
	SOCK_CLOSE(pr3287_ls);
	return;
    }
    syncopt = lazyaf("%s %d", OptSyncPort, ntohs(pr3287_lsa.sin_port));
    if (listen(pr3287_ls, 5) < 0) {
	popup_a_sockerr("listen(printer sync)");
	SOCK_CLOSE(pr3287_ls);
	return;
    }
#if defined(_WIN32) /*[*/
    pr3287_ls_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (pr3287_ls_handle == NULL) {
	popup_an_error("CreateEvent: %s", win32_strerror(GetLastError()));
	SOCK_CLOSE(pr3287_ls);
	pr3287_ls = INVALID_SOCKET;
	return;
    }
    if (WSAEventSelect(pr3287_ls, pr3287_ls_handle, FD_ACCEPT) != 0) {
	popup_an_error("WSAEventSelect: %s", win32_strerror(GetLastError()));
	SOCK_CLOSE(pr3287_ls);
	pr3287_ls = INVALID_SOCKET;
	return;
    }
    pr3287_ls_id = AddInput(pr3287_ls_handle, pr3287_accept);
#else /*][*/
    pr3287_ls_id = AddInput(pr3287_ls, pr3287_accept);
#endif /*]*/

    /* Fetch the command line and command resources. */
    cmdline = get_resource(cmdlineName);
    if (cmdline == NULL) {
	popup_an_error("%s resource not defined", cmdlineName);
	SOCK_CLOSE(pr3287_ls);
	return;
    }
#if !defined(_WIN32) /*[*/
    cmd = get_resource(ResPrinterCommand);
    if (cmd == NULL) {
	popup_an_error(ResPrinterCommand " resource not defined");
	SOCK_CLOSE(pr3287_ls);
	return;
    }
#else /*][*/
    printerName = get_resource(ResPrinterName);
#endif /*]*/

    /* Construct the charset option. */
    charset_cmd = lazyaf("-charset %s", get_charset_name());

    /* Construct proxy option. */
    if (appres.proxy != NULL) {
#if !defined(_WIN32) /*[*/
	proxy_cmd = lazyaf("-proxy \"%s\"", appres.proxy);
#else /*][ */
	proxy_cmd = lazyaf("-proxy %s", appres.proxy);
#endif /*]*/
    }

#if defined(_WIN32) /*[*/
    /* Get the codepage for the printer. */
    pcp_res = get_resource(ResPrinterCodepage);
    if (pcp_res) {
	printercp = lazyaf("-printercp %s", pcp_res);
    }
#endif /*]*/

    /* Get printer options. */
    pr3287_opts = appres.interactive.printer_opts;
    if (pr3287_opts == NULL) {
	pr3287_opts = get_resource(ResPrinterOptions);
    }

    /* Construct the command line. */

    /* Substitute. */
    vb_init(&r);
    for (s = cmdline; (c = *s) != '\0'; s++) {

	if (c == '%') {
	    if (!strncmp(s+1, "L%", 2)) {
		vb_appends(&r, lu);
		s += 2;
		continue;
	    } else if (!strncmp(s+1, "H%", 2)) {
		vb_appends(&r, qualified_host);
		s += 2;
		continue;
#if !defined(_WIN32) /*[*/
	    } else if (!strncmp(s+1, "C%", 2)) {
		vb_appends(&r, cmd);
		s += 2;
		continue;
#endif /*]*/
	    } else if (!strncmp(s+1, "R%", 2)) {
		vb_appends(&r, charset_cmd);
		s += 2;
		continue;
	    } else if (!strncmp(s+1, "P%", 2)) {
		if (proxy_cmd != NULL) {
		    vb_appends(&r, proxy_cmd);
		}
		s += 2;
		continue;
#if defined(_WIN32) /*[*/
	    } else if (!strncmp(s+1, "I%", 2)) {
		if (printercp != NULL) {
		    vb_appends(&r, printercp);
		}
		s += 2;
		continue;
#endif /*]*/
	    } else if (!strncmp(s+1, "O%", 2)) {
		if (pr3287_opts != NULL) {
		    vb_appends(&r, pr3287_opts);
		}
		s += 2;
		continue;
	    } else if (!strncmp(s+1, "V%", 2)) {
#if defined(HAVE_LIBSSL) /*[*/
		if (appres.ssl.verify_host_cert) {
		    vb_appends(&r, " " OptVerifyHostCert);
		}
		if (appres.ssl.self_signed_ok) {
		    vb_appends(&r, " " OptSelfSignedOk);
		}
		if (appres.ssl.ca_dir) {
		    vb_appendf(&r, " %s \"%s\"", OptCaDir, appres.ssl.ca_dir);
		}
		if (appres.ssl.ca_file) {
		    vb_appendf(&r, " %s \"%s\"", OptCaFile,
			    appres.ssl.ca_file);
		}
		if (appres.ssl.cert_file) {
		    vb_appendf(&r, " %s \"%s\"", OptCertFile,
			    appres.ssl.cert_file);
		}
		if (appres.ssl.cert_file_type) {
		    vb_appendf(&r, " %s %s", OptCertFileType,
			    appres.ssl.cert_file_type);
		}
		if (appres.ssl.chain_file) {
		    vb_appendf(&r, " %s \"%s\"", OptChainFile,
			    appres.ssl.chain_file);
		}
		if (appres.ssl.key_file) {
		    vb_appendf(&r, " %s \"%s\"", OptKeyFile,
			    appres.ssl.key_file);
		}
		if (appres.ssl.key_passwd) {
		    vb_appendf(&r, " %s \"%s\"", OptKeyPasswd,
			    appres.ssl.key_passwd);
		}
		if (appres.ssl.accept_hostname) {
		    vb_appendf(&r, " %s \"%s\"", OptAcceptHostname,
			    appres.ssl.accept_hostname);
		}
#endif /*]*/
		s += 2;
		continue;
	    } else if (!strncmp(s+1, "S%", 2)) {
		vb_appends(&r, syncopt);
		s += 2;
		continue;
	    }
	}
	vb_append(&r, &c, 1);
    }
    cmd_text = vb_consume(&r);

#if !defined(_WIN32) /*[*/
    vtrace("Printer command: %s\n", cmd_text);

    /* Make pipes for printer's stdout and stderr. */
    if (pipe(stdout_pipe) < 0) {
	popup_an_errno(errno, "pipe() failed");
	Free(cmd_text);
	SOCK_CLOSE(pr3287_ls);
	return;
    }
    (void) fcntl(stdout_pipe[0], F_SETFD, 1);
    if (pipe(stderr_pipe) < 0) {
	popup_an_errno(errno, "pipe() failed");
	(void) close(stdout_pipe[0]);
	(void) close(stdout_pipe[1]);
	Free(cmd_text);
	SOCK_CLOSE(pr3287_ls);
	return;
    }
    (void) fcntl(stderr_pipe[0], F_SETFD, 1);

    /* Fork and exec the printer session. */
    switch (pr3287_pid = fork()) {
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
	pr3287_stdout.fd = stdout_pipe[0];
	(void) close(stderr_pipe[1]);
	pr3287_stderr.fd = stderr_pipe[0];
	pr3287_stdout.input_id = AddInput(pr3287_stdout.fd, pr3287_output);
	pr3287_stderr.input_id = AddInput(pr3287_stderr.fd, pr3287_error);
	++children;
	break;
    case -1:	/* error */
	popup_an_errno(errno, "fork()");
	(void) close(stdout_pipe[0]);
	(void) close(stdout_pipe[1]);
	(void) close(stderr_pipe[0]);
	(void) close(stderr_pipe[1]);
	success = false;
	break;
    }
#else /*][*/
/* Pass the command via the environment. */
    if (printerName != NULL) {
	putenv(lazyaf("PRINTER=%s", printerName));
    }

    /* Create the wpr3287 process. */
    if (!strncasecmp(cmd_text, "wpr3287.exe", 11)) {
	cp_cmdline = lazyaf("%s%s", instdir, cmd_text);
    } else {
	cp_cmdline = cmd_text;
    }

    vtrace("Printer command: %s\n", cp_cmdline);
    if (printerName != NULL) {
	vtrace("Printer (via %%PRINTER%%): %s\n", printerName);
    }
    memset(&si, '\0', sizeof(si));
    si.cb = sizeof(pi);
    memset(&pi, '\0', sizeof(pi));
    if (!CreateProcess(NULL, cp_cmdline, NULL, NULL, FALSE, DETACHED_PROCESS,
		NULL, NULL, &si, &pi)) {
	popup_an_error("CreateProcess() for printer session failed: %s",
		win32_strerror(GetLastError()));
	success = false;
    } else {
	pr3287_handle = pi.hProcess;
	CloseHandle(pi.hThread);
    }
#endif /*]*/

    Free(cmd_text);

    /* Tell everyone else. */
    if (success) {
	pr3287_state = P_RUNNING;
	st_changed(ST_PRINTER, true);
    }
}

#if !defined(_WIN32) /*[*/
/* There's data from the printer session. */
static void
pr3287_data(struct pr3o *p, bool is_err)
{
    int space;
    int nr;
    static char exitmsg[] = "Printer session exited";

    /* Read whatever there is. */
    space = PRINTER_BUF - p->count - 1;
    nr = read(p->fd, p->buf + p->count, space);

    /* Handle read errors and end-of-file. */
    if (nr < 0) {
	popup_an_errno(errno, "Printer session pipe input failed");
	pr3287_session_stop();
	return;
    }
    if (nr == 0) {
	vtrace("Printer session %s EOF.\n", is_err? "stderr": "stdout");
	if (pr3287_stderr.timeout_id != NULL_IOID) {
	    /*
	     * Append a termination error message to whatever the
	     * printer process said, and pop it up.
	     */
	    p = &pr3287_stderr;
	    space = PRINTER_BUF - p->count - 1;
	    if (p->count && *(p->buf + p->count - 1) != '\n') {
		*(p->buf + p->count) = '\n';
		p->count++;
		space--;
	    }
	    (void) strncpy(p->buf + p->count, exitmsg, space);
	    p->count += strlen(exitmsg);
	    if (p->count >= PRINTER_BUF) {
		p->count = PRINTER_BUF - 1;
	    }
	    pr3287_dump(p, true, true);
	} else {
	    popup_an_error("%s", exitmsg);
	}

	/* Now that we've gotten EOF, make sure we stop the process. */
	pr3287_session_stop();
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
	pr3287_dump(p, is_err, false);
    } else if (p->timeout_id == NULL_IOID) {
	p->timeout_id = AddTimeOut(1000,
		is_err? pr3287_etimeout: pr3287_otimeout);
    }
}

/* The printer process has some output for us. */
static void
pr3287_output(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    pr3287_data(&pr3287_stdout, false);
}

/* The printer process has some error output for us. */
static void
pr3287_error(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    pr3287_data(&pr3287_stderr, true);
}

/* Timeout from printer output or error output. */
static void
pr3287_timeout(struct pr3o *p, bool is_err)
{
    /* Forget the timeout ID. */
    p->timeout_id = NULL_IOID;

    /* Dump the output. */
    pr3287_dump(p, is_err, false);
}

/* Timeout from printer output. */
static void
pr3287_otimeout(ioid_t id _is_unused)
{
    pr3287_timeout(&pr3287_stdout, false);
}

/* Timeout from printer error output. */
static void
pr3287_etimeout(ioid_t id _is_unused)
{
    pr3287_timeout(&pr3287_stderr, true);
}

/* Dump pending printer process output. */
static void
pr3287_dump(struct pr3o *p, bool is_err, bool is_dead)
{
    if (p->count) {
	/*
	 * Strip any trailing newline, and make sure the buffer is
	 * NULL terminated.
	 */
	if (p->buf[p->count - 1] == '\n') {
	    p->buf[--(p->count)] = '\0';
	} else if (p->buf[p->count]) {
	    p->buf[p->count] = '\0';
	}

	/* Dump it and clear the buffer. */
	popup_printer_output(is_err, is_dead? NULL: pr3287_session_stop, "%s",
		p->buf);
	p->count = 0;
    }
}
#endif /*]*/

/* Shut off printer sync input. */
static void
pr3287_stop_sync(void)
{
    assert(pr3287_sync_id != NULL_IOID);
    RemoveInput(pr3287_sync_id);
    pr3287_sync_id = NULL_IOID;
#if defined(_WIN32) /*[*/
    assert(pr3287_sync_handle != NULL);
    CloseHandle(pr3287_sync_handle);
    pr3287_sync_handle = NULL;
#endif /*]*/
    SOCK_CLOSE(pr3287_sync);
    pr3287_sync = INVALID_SOCKET;
}

/* Input from pr3287 on the synchronization socket. */
static void
pr3287_sync_input(iosrc_t fd _is_unused, ioid_t id _is_unused)
{
    vtrace("Input or EOF on printer sync socket.\n");
    assert(pr3287_state >= P_RUNNING);

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
    pr3287_stop_sync();
}

/* Shut off the printer sync listening socket. */
static void
pr3287_stop_listening(void)
{
    assert(pr3287_ls_id != NULL_IOID);
    assert(pr3287_ls != INVALID_SOCKET);
#if defined(_WIN32) /*[*/
    assert(pr3287_ls_handle != NULL);
#endif /*]*/

    RemoveInput(pr3287_ls_id);
    pr3287_ls_id = NULL_IOID;
#if defined(_WIN32) /*[*/
    CloseHandle(pr3287_ls_handle);
    pr3287_ls_handle = NULL;
#endif /*]*/
    SOCK_CLOSE(pr3287_ls);
    pr3287_ls = INVALID_SOCKET;
}

/* Accept a synchronization connection from pr3287. */
static void
pr3287_accept(iosrc_t fd _is_unused, ioid_t id)
{
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);

    /* Accept the connection. */
    assert(pr3287_state == P_RUNNING);
    pr3287_sync = accept(pr3287_ls, (struct sockaddr *)&sin, &len);
    if (pr3287_sync == INVALID_SOCKET) {
	popup_a_sockerr("accept(printer sync)");
    } else {
	vtrace("Accepted sync connection from printer.\n");

#if defined(_WIN32) /*[*/
	pr3287_sync_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (pr3287_sync_handle == NULL) {
	    popup_an_error("CreateEvent failed");
	    x3270_exit(1);
	}
	if (WSAEventSelect(pr3287_sync, pr3287_sync_handle,
		    FD_READ | FD_CLOSE) != 0) {
	    popup_an_error("Can't set socket handle events\n");
	    x3270_exit(1);
	}
	pr3287_sync_id = AddInput(pr3287_sync_handle, pr3287_sync_input);
#else /*][*/
	pr3287_sync_id = AddInput(pr3287_sync, pr3287_sync_input);
#endif /*]*/
    }

    /* No more need for the listening socket. */
    pr3287_stop_listening();
}

/* Clean up all connections to pr3287. */
static void
pr3287_cleanup_io(void)
{
    /* Remove inputs. */
    if (pr3287_stdout.input_id) {
	RemoveInput(pr3287_stdout.input_id);
	pr3287_stdout.input_id = NULL_IOID;
    }
    if (pr3287_stderr.input_id) {
	RemoveInput(pr3287_stderr.input_id);
	pr3287_stderr.input_id = NULL_IOID;
    }

    /* Cancel timeouts. */
    if (pr3287_stdout.timeout_id != NULL_IOID) {
	RemoveTimeOut(pr3287_stdout.timeout_id);
	pr3287_stdout.timeout_id = NULL_IOID;
    }
    if (pr3287_stderr.timeout_id != NULL_IOID) {
	RemoveTimeOut(pr3287_stderr.timeout_id);
	pr3287_stderr.timeout_id = NULL_IOID;
    }

    /* Clear buffers. */
    pr3287_stdout.count = 0;
    pr3287_stderr.count = 0;

    /*
     * If we have a sync socket connection, shut it down to signal pr3287
     * to exit gracefully.
     */
    if (pr3287_sync != INVALID_SOCKET) {
	vtrace("Stopping printer by shutting down sync socket.\n");
	assert(pr3287_ls == INVALID_SOCKET);

	/* The separate shutdown() call is likely redundant. */
#if !defined(_WIN32) /*[*/
	shutdown(pr3287_sync, SHUT_WR);
#else /*][*/
	shutdown(pr3287_sync, SD_SEND);
#endif /*]*/

	/* We no longer care about printer sync input. */
	pr3287_stop_sync();
    } else if (pr3287_ls_id != NULL_IOID) {
	/* Stop listening for sync connections. */
	pr3287_stop_listening();
    }
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
pr3287_session_check(
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

    if (pr3287_state == P_NONE) {
	return;
    }

#if !defined(_WIN32) /*[*/
    if (pid != pr3287_pid) {
	return;
    }

    /*
     * If we didn't stop it on purpose, decode and display the printer's
     * exit status.
     */
    if (pr3287_state == P_RUNNING) {
	if (WIFEXITED(status)) {
	    popup_an_error("Printer process exited with status %d",
		    WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
	    popup_an_error("Printer process killed by signal %d",
		    WTERMSIG(status));
	} else {
	    popup_an_error("Printer process stopped by unknown status %d",
		    status);
	}
    }
    pr3287_pid = -1;
#else /*][*/

    if (pr3287_handle != NULL &&
	    GetExitCodeProcess(pr3287_handle, &exit_code) != 0 &&
	    exit_code != STILL_ACTIVE) {

	CloseHandle(pr3287_handle);
	pr3287_handle = NULL;

	if (pr3287_state == P_RUNNING) {
	    popup_an_error("Printer process exited with status 0x%lx",
		    (long)exit_code);
	}
    } else {
	/* It is still running. */
	return;
    }
#endif /*]*/

    vtrace("Printer session exited.\n");

    /* Stop any pending printer kill request. */
    if (pr3287_state == P_SHUTDOWN) {
	assert(pr3287_kill_id != NULL_IOID);
	RemoveTimeOut(pr3287_kill_id);
	pr3287_kill_id = NULL_IOID;
	pr3287_state = P_NONE;
    }

    /* No need for sync input any more. */
    if (pr3287_sync_id != NULL_IOID) {
	pr3287_stop_sync();
    }

    /*
     * Clean up I/O.
     * It would be better to wait for EOF first so we can display errors from
     * pr3287, but for now, we just need to get the state straight.
     */
    pr3287_cleanup_io();

    pr3287_state = P_NONE;

    /* Propagate the state. */
    st_changed(ST_PRINTER, false);

    /*
     * If there is a pending request to start the printer, set a timeout to
     * start it.
     */
    if (pr3287_delay_lu != NULL) {
	pr3287_state = P_DELAY;
	pr3287_delay_id = AddTimeOut(PRINTER_DELAY_MS, delayed_start);
    }
}

/* Terminate pr3287, with prejudice. */
static void
pr3287_kill(ioid_t id _is_unused)
{
    vtrace("Forcibly terminating printer session.\n");

    /* Kill the process. */
#if defined(_WIN32) /*[*/
    assert(pr3287_handle != NULL);
    TerminateProcess(pr3287_handle, 0);
#else /*][*/
    assert(pr3287_pid != -1);
    (void) kill(-pr3287_pid, SIGTERM);
#endif /*]*/

    pr3287_kill_id = NULL_IOID;
    pr3287_state = P_TERMINATING;
}


/* Close the printer session. */
void
pr3287_session_stop()
{
    switch (pr3287_state) {
    case P_DELAY:
	vtrace("Canceling delayed printer session start.\n");
	assert(pr3287_delay_id != NULL_IOID);
	RemoveTimeOut(pr3287_delay_id);
	pr3287_delay_id = NULL_IOID;
	assert(pr3287_delay_lu != NULL);
	Free(pr3287_delay_lu);
	pr3287_delay_lu = NULL;
	break;
    case P_RUNNING:
	/* Run through the logic below. */
	break;
    default:
	/* Nothing interesting to do. */
	return;
    }

    vtrace("Stopping printer session.\n");

    pr3287_cleanup_io();

    /* Set a timeout to terminate it not so gracefully. */
    pr3287_state = P_SHUTDOWN;
    pr3287_kill_id = AddTimeOut(PRINTER_KILL_MS, pr3287_kill);
}

/* The emulator is exiting.  Make sure the printer session is cleaned up. */
static void
pr3287_exiting(bool b _is_unused)
{
    if (pr3287_state >= P_RUNNING && pr3287_state < P_TERMINATING) {
	pr3287_kill(NULL_IOID);
    }
}

/* Host connect/disconnect/3270-mode event. */
static void
pr3287_host_connect(bool connected _is_unused)
{
    if (IN_3270) {
	char *pr3287_lu = appres.interactive.printer_lu;

	if (pr3287_lu != NULL && !pr3287_session_running()) {
	    if (!strcmp(pr3287_lu, ".")) {
		if (IN_TN3270E) {
		    /* Associate with TN3270E session. */
		    pr3287_session_start(NULL);
		}
	    } else {
		/* Specific LU. */
		pr3287_session_start(pr3287_lu);
	    }
	} else if (!IN_E && pr3287_lu != NULL && !strcmp(pr3287_lu, ".") &&
		pr3287_session_running()) {

	    /* Stop an automatic associated printer. */
	    pr3287_session_stop();
	}
    } else if (pr3287_session_running()) {
	/*
	 * We're no longer in 3270 mode, then we can no longer have a
	 * printer session.  This may cause some fireworks if there is
	 * a print job pending when we do this, so some sort of awful
	 * timeout may be needed.
	 */
	pr3287_session_stop();
    } else {
	/*
	 * Forget state associated with printer start-up.
	 */
	if (pr3287_state == P_DELAY) {
	    pr3287_state = P_NONE;
	}
	if (pr3287_delay_id != NULL_IOID) {
	    RemoveTimeOut(pr3287_delay_id);
	    pr3287_delay_id = NULL_IOID;
	}
	if (pr3287_delay_lu != NULL) {
	    Free(pr3287_delay_lu);
	    pr3287_delay_lu = NULL;
	}
    }
}

bool
pr3287_session_running(void)
{
    return (pr3287_state == P_RUNNING);
}
