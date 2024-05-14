/*
 * Copyright (c) 2000-2024 Paul Mattes.
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

#include "codepage.h"
#include "host.h"
#include "opts.h"
#include "popups.h"
#include "pr3287_session.h"
#include "telnet_core.h"
#include "sio.h"
#include "telnet.h"
#include "toggles.h"
#include "trace.h"
#include "txa.h"
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
static ioid_t	pr3287_id = NULL_IOID;
static enum {
    PRS_NONE,		/* no printer session */
    PRS_DELAY,		/* delay before (re)starting pr3287 */
    PRS_RUNNING,		/* pr3287 process running */
    PRS_SHUTDOWN,		/* pr3287 graceful shutdown requested */
    PRS_TERMINATING	/* pr3287 forcible termination requested */
} pr3287_state = PRS_NONE;
static socket_t	pr3287_ls = INVALID_SOCKET;	/* printer sync listening socket */
static ioid_t	pr3287_ls_id = NULL_IOID; /* input ID */
#if defined(_WIN32) /*[*/
static HANDLE	pr3287_ls_handle = NULL;
#endif /*]*/
static socket_t	pr3287_sync = INVALID_SOCKET;	/* printer sync socket */
static ioid_t	pr3287_sync_id = NULL_IOID; /* input ID */
#if defined(_WIN32) /*[*/
static HANDLE	pr3287_sync_handle = NULL;
static HANDLE	pr3287_stderr_wr = NULL;
static HANDLE	pr3287_stderr_rd = NULL;
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
static bool	pr3287_associated = false;
static char	*pr3287_running_lu;
static int	printer_delay_ms;

#if !defined(_WIN32) /*[*/
static void	pr3287_output(iosrc_t fd, ioid_t id);
static void	pr3287_error(iosrc_t fd, ioid_t id);
static void	pr3287_otimeout(ioid_t id);
static void	pr3287_etimeout(ioid_t id);
static void	pr3287_dump(struct pr3o *p, bool is_err, bool is_dead);
static void	pr3287_session_check(pid_t pid, int status);
#else /*][*/
static void	pr3287_session_check(void);
#endif /*]*/
static void	pr3287_host_connect(bool connected _is_unused);
static void	pr3287_exiting(bool b _is_unused);
static void	pr3287_accept(iosrc_t fd, ioid_t id);
static void	pr3287_start_now(const char *lu, bool associated);
static toggle_upcall_ret_t pr3287_toggle_lu(const char *name, const char *value, unsigned flags, ia_t ia);
#if defined(_WIN32) /*[*/
static toggle_upcall_ret_t pr3287_toggle_name(const char *name, const char *value, unsigned flags, ia_t ia);
static toggle_upcall_ret_t pr3287_toggle_codepage(const char *name, const char *value, unsigned flags, ia_t ia);
#endif /*]*/
static toggle_upcall_ret_t pr3287_toggle_opts(const char *name, const char *value, unsigned flags, ia_t ia);

/* Globals */

/**
 * Printer session module registration.
 */
void
pr3287_session_register(void)
{
    static opt_t pr3287_session_opts[] = {
	{ OptPrinterLu,OPT_STRING,  false, ResPrinterLu,
	    aoffset(interactive.printer_lu),
	    "<luname>",
	    "Automatically start a pr3287 printer session to <luname>" },
    };
    static res_t pr3287_session_resources[] = {
	{ ResPrinterLu, aoffset(interactive.printer_lu),XRM_STRING },
        { ResPrinterOptions,aoffset(interactive.printer_opts),XRM_STRING },
    };
    static xres_t pr3287_session_xresources[] = {
	{ ResAssocCommand,	V_FLAT },
	{ ResLuCommandLine,	V_FLAT },
#if defined(_WIN32) /*[*/
	{ ResPrinterCodepage,	V_FLAT },
#endif /*]*/
	{ ResPrinterCommand,	V_FLAT },
#if defined(_WIN32) /*[*/
	{ ResPrinterName,	V_FLAT },
#endif /*]*/
    };

    /* Register interest in host connects and mode changes. */
    register_schange(ST_CONNECT, pr3287_host_connect);
    register_schange(ST_3270_MODE, pr3287_host_connect);
    register_schange(ST_EXITING, pr3287_exiting);

    /* Register the extended toggles. */
    register_extended_toggle(ResPrinterLu, pr3287_toggle_lu, NULL, NULL,
	    (void **)&appres.interactive.printer_lu, XRM_STRING);
#if defined(_WIN32) /*[*/
    register_extended_toggle(ResPrinterName, pr3287_toggle_name, NULL, NULL,
	    NULL, XRM_STRING);
    register_extended_toggle(ResPrinterCodepage, pr3287_toggle_codepage, NULL,
	    NULL, NULL, XRM_STRING);
#endif /*]*/
    register_extended_toggle(ResPrinterOptions, pr3287_toggle_opts, NULL, NULL,
	    NULL, XRM_STRING);

    /* Register options. */
    register_opts(pr3287_session_opts, array_count(pr3287_session_opts));

    /* Register resources. */
    register_resources(pr3287_session_resources,
	    array_count(pr3287_session_resources));
    register_xresources(pr3287_session_xresources,
	    array_count(pr3287_session_xresources));
}

#if defined(_WIN32) /*[*/
/*
 * Read pr3287's stdout/stderr when it exits.
 */
static char *
read_pr3287_errors(void)
{
    DWORD nread;
    CHAR buf[PRINTER_BUF]; 
    BOOL success = FALSE;
    char *result = NULL;
    size_t result_len = 0;

    for (;;) {
	DWORD ix;

	success = ReadFile(pr3287_stderr_rd, buf, PRINTER_BUF, &nread, NULL);
	if (!success || nread == 0) {
	    break; 
	}

	result = Realloc(result, result_len + nread + 1);
	for (ix = 0; ix < nread; ix++) {
	    if (buf[ix] != '\r') {
		result[result_len++] = buf[ix];
	    }
	}
    } 
    if (result_len > 0) {
	result[result_len] = '\0';
    }
    return result;
}
#endif /*]*/

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
    char *stderr_text;
#endif /*]*/

    assert(pr3287_state == PRS_TERMINATING);

    vtrace("Waiting for old printer session to exit.\n");
#if !defined(_WIN32) /*[*/
    if (waitpid(pr3287_pid, &status, 0) < 0) {
	popup_an_errno(errno, "Printer process waitpid() failed");
	return;
    }
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
    CloseHandle(pr3287_stderr_wr);
    pr3287_stderr_wr = NULL;

    stderr_text = read_pr3287_errors();
    CloseHandle(pr3287_stderr_rd);
    pr3287_stderr_rd = NULL;

    if (exit_code != 0) {
	popup_printer_output(true, NULL,
		"%s%sPrinter process exited with status 0x%lx",
		(stderr_text != NULL)? stderr_text: "",
		(stderr_text != NULL)? "\n": "",
		(long)exit_code);
    } else if (stderr_text != NULL) {
	popup_printer_output(true, NULL, "%s", stderr_text);
    }
    if (stderr_text != NULL) {
	Free(stderr_text);
    }
#endif /*]*/

    vtrace("Old printer session exited.\n");
    pr3287_state = PRS_NONE;
    st_changed(ST_PRINTER, false);
}

/* Delayed start function. */
static void
delayed_start(ioid_t id _is_unused)
{
    assert(pr3287_state == PRS_DELAY);

    vtrace("Printer session start delay complete.\n");

    /* Start the printer. */
    pr3287_state = PRS_NONE;
    assert(pr3287_delay_lu != NULL);
    pr3287_start_now(pr3287_delay_lu, pr3287_delay_associated);

    /* Forget the saved state. */
    pr3287_delay_id = NULL_IOID;
    Free(pr3287_delay_lu);
    pr3287_delay_lu = NULL;
}

static int
get_printer_delay_ms(void)
{
    if (printer_delay_ms == 0) {
	char *s = getenv("PRINTER_DELAY_MS");

	if (s != NULL) {
	    printer_delay_ms = atoi(s);
	}
	if (printer_delay_ms <= 0) {
	    printer_delay_ms = PRINTER_DELAY_MS;
	}
    }
    return printer_delay_ms;
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
    pr3287_associated = false;

    /* Gotta be in 3270 mode. */
    if (!IN_3270) {
	popup_an_error("Not in 3270 mode");
	return;
    }

    /* Figure out the LU. */
    if (lu == NULL) {
	/* Associate with the current session. */
	pr3287_associated = true;

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
    case PRS_NONE:
	/*
	 * Remember what was requested, and set a timeout to start the
	 * new session.
	 */
	vtrace("Delaying printer session start %dms.\n",
		get_printer_delay_ms());
	Replace(pr3287_delay_lu, NewString(lu));
	pr3287_delay_associated = pr3287_associated;
	pr3287_state = PRS_DELAY;
	pr3287_delay_id = AddTimeOut(get_printer_delay_ms(), delayed_start);
	break;
    case PRS_DELAY:
    case PRS_RUNNING:
	/* Redundant start request. */
	popup_an_error("Printer is already started or running");
	return;
    case PRS_SHUTDOWN:
	/*
	 * Remember what was requested, and let the state change or
	 * timeout functions start the new session.
	 *
	 * There is a window here where two manual start commands could
	 * get in after a manual stop. This is needed because we can't
	 * distinguish a manual from an automatic start.
	 */
	vtrace("Delaying printer session start %dms after exit.\n",
		get_printer_delay_ms());
	Replace(pr3287_delay_lu, NewString(lu));
	pr3287_delay_associated = pr3287_associated;
	return;
    case PRS_TERMINATING:
	/* Collect the exit status now and start the new session. */
	pr3287_reap_now();
	pr3287_start_now(lu, pr3287_associated);
	break;
    }
}

#if !defined(_WIN32) /*[*/
/**
 * Callback for pr3287 session exit.
 *
 * @param[in] id	I/O identifier (unused)
 * @param[in] status	exit status
 */
static void
pr3287_reaped(ioid_t id _is_unused, int status)
{
    pr3287_id = NULL_IOID;
    pr3287_session_check(pr3287_pid, status);
}
#else /*][*/
/**
 * Callback for pr3287 session exit.
 *
 * @param[in] iosrc	I/O source (unused)
 * @param[in] id	I/O identifier (unused)
 */
static void
pr3287_reaped(iosrc_t iosrc _is_unused, ioid_t id _is_unused)
{
    RemoveInput(pr3287_id);
    pr3287_id = NULL_IOID;
    pr3287_session_check();
}
#endif /*]*/

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
    SECURITY_ATTRIBUTES sa;
    DWORD mode;
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

    assert(pr3287_state == PRS_NONE);

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
	pr3287_ls = INVALID_SOCKET;
	return;
    }
    memset(&pr3287_lsa, '\0', sizeof(pr3287_lsa));
    pr3287_lsa.sin_family = AF_INET;
    len = sizeof(pr3287_lsa);
    if (getsockname(pr3287_ls, (struct sockaddr *)&pr3287_lsa, &len) < 0) {
	popup_a_sockerr("getsockname(printer sync)");
	SOCK_CLOSE(pr3287_ls);
	pr3287_ls = INVALID_SOCKET;
	return;
    }
    syncopt = txAsprintf("%s %d", OptSyncPort, ntohs(pr3287_lsa.sin_port));
    if (listen(pr3287_ls, 5) < 0) {
	popup_a_sockerr("listen(printer sync)");
	SOCK_CLOSE(pr3287_ls);
	pr3287_ls = INVALID_SOCKET;
	return;
    }
#if !defined(_WIN32) /*[*/
    fcntl(pr3287_ls, F_SETFD, 1);
#endif /*]*/
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
	pr3287_ls = INVALID_SOCKET;
	RemoveInput(pr3287_ls_id);
	return;
    }
#if !defined(_WIN32) /*[*/
    cmd = get_resource(ResPrinterCommand);
    if (cmd == NULL) {
	popup_an_error(ResPrinterCommand " resource not defined");
	SOCK_CLOSE(pr3287_ls);
	pr3287_ls = INVALID_SOCKET;
	RemoveInput(pr3287_ls_id);
	return;
    }
#else /*][*/
    printerName = get_resource(ResPrinterName);
#endif /*]*/

    /* Construct the charset option. */
    charset_cmd = txAsprintf(OptCodePage " %s", get_codepage_name());

    /* Construct proxy option. */
    if (appres.proxy != NULL) {
#if !defined(_WIN32) /*[*/
	proxy_cmd = txAsprintf(OptProxy  " \"%s\"", appres.proxy);
#else /*][ */
	proxy_cmd = txAsprintf(OptProxy " %s", appres.proxy);
#endif /*]*/
    }

#if defined(_WIN32) /*[*/
    /* Get the codepage for the printer. */
    pcp_res = get_resource(ResPrinterCodepage);
    if (pcp_res) {
	printercp = txAsprintf("-printercp %s", pcp_res);
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
		const char *spc = "";
		if (appres.prefer_ipv4) {
		    vb_appendf(&r, " %s", OptPreferIpv4);
		    spc = " ";
		}
		if (appres.prefer_ipv6) {
		    vb_appendf(&r, " %s", OptPreferIpv6);
		    spc = " ";
		}
		vb_appendf(&r, "%s%s", spc, qualified_host);
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
		unsigned tls_opts = sio_all_options_supported();

		if ((tls_opts & TLS_OPT_VERIFY_HOST_CERT) &&
			!appres.tls.verify_host_cert) {
		    vb_appends(&r, " " OptNoVerifyHostCert);
		}
		if ((tls_opts & TLS_OPT_CA_DIR) && appres.tls.ca_dir) {
		    vb_appendf(&r, " %s \"%s\"", OptCaDir, appres.tls.ca_dir);
		}
		if ((tls_opts & TLS_OPT_CA_FILE) && appres.tls.ca_file) {
		    vb_appendf(&r, " %s \"%s\"", OptCaFile,
			    appres.tls.ca_file);
		}
		if ((tls_opts & TLS_OPT_CERT_FILE) && appres.tls.cert_file) {
		    vb_appendf(&r, " %s \"%s\"", OptCertFile,
			    appres.tls.cert_file);
		}
		if ((tls_opts & TLS_OPT_CERT_FILE_TYPE) &&
			appres.tls.cert_file_type) {
		    vb_appendf(&r, " %s %s", OptCertFileType,
			    appres.tls.cert_file_type);
		}
		if ((tls_opts & TLS_OPT_CHAIN_FILE) && appres.tls.chain_file) {
		    vb_appendf(&r, " %s \"%s\"", OptChainFile,
			    appres.tls.chain_file);
		}
		if ((tls_opts & TLS_OPT_KEY_FILE) && appres.tls.key_file) {
		    vb_appendf(&r, " %s \"%s\"", OptKeyFile,
			    appres.tls.key_file);
		}
		if ((tls_opts & TLS_OPT_KEY_PASSWD) && appres.tls.key_passwd) {
		    vb_appendf(&r, " %s \"%s\"", OptKeyPasswd,
			    appres.tls.key_passwd);
		}
		if ((tls_opts & TLS_OPT_CLIENT_CERT) &&
			appres.tls.client_cert) {
		    vb_appendf(&r, " %s %s", OptClientCert,
			    appres.tls.client_cert);
		}
		if ((tls_opts & TLS_OPT_ACCEPT_HOSTNAME) &&
			appres.tls.accept_hostname) {
		    vb_appendf(&r, " %s \"%s\"", OptAcceptHostname,
			    appres.tls.accept_hostname);
		}
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
	pr3287_ls = INVALID_SOCKET;
	RemoveInput(pr3287_ls_id);
	return;
    }
    fcntl(stdout_pipe[0], F_SETFD, 1);
    if (pipe(stderr_pipe) < 0) {
	popup_an_errno(errno, "pipe() failed");
	close(stdout_pipe[0]);
	close(stdout_pipe[1]);
	Free(cmd_text);
	SOCK_CLOSE(pr3287_ls);
	pr3287_ls = INVALID_SOCKET;
	RemoveInput(pr3287_ls_id);
	return;
    }
    fcntl(stderr_pipe[0], F_SETFD, 1);

    /* Fork and exec the printer session. */
    switch (pr3287_pid = fork()) {
    case 0:	/* child process */
	dup2(stdout_pipe[1], 1);
	close(stdout_pipe[1]);
	dup2(stderr_pipe[1], 2);
	close(stderr_pipe[1]);
	if (setsid() < 0) {
	    perror("setsid");
	    _exit(1);
	}
	execlp("/bin/sh", "sh", "-c", cmd_text, NULL);
	perror("exec(printer)");
	_exit(1);
    default:	/* parent process */
	close(stdout_pipe[1]);
	pr3287_stdout.fd = stdout_pipe[0];
	close(stderr_pipe[1]);
	pr3287_stderr.fd = stderr_pipe[0];
	pr3287_stdout.input_id = AddInput(pr3287_stdout.fd, pr3287_output);
	pr3287_stderr.input_id = AddInput(pr3287_stderr.fd, pr3287_error);
	pr3287_id = AddChild(pr3287_pid, pr3287_reaped);
	break;
    case -1:	/* error */
	popup_an_errno(errno, "fork()");
	close(stdout_pipe[0]);
	close(stdout_pipe[1]);
	close(stderr_pipe[0]);
	close(stderr_pipe[1]);
	success = false;
	break;
    }
#else /*][*/
/* Pass the command via the environment. */
    if (printerName != NULL) {
	putenv(txAsprintf("PRINTER=%s", printerName));
    }

    /* Create the pr3287 process. */
    if (!strncasecmp(cmd_text, "pr3287.exe", 10) ||
	    !strncasecmp(cmd_text, "wpr3287.exe", 11)) {
	cp_cmdline = txAsprintf("%s%s", instdir, cmd_text);
    } else {
	cp_cmdline = cmd_text;
    }

    vtrace("Printer command: %s\n", cp_cmdline);
    if (printerName != NULL) {
	vtrace("Printer (via %%PRINTER%%): %s\n", printerName);
    }

    /* Create a named pipe for pr3287's stderr. */
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
    sa.bInheritHandle = TRUE; 
    sa.lpSecurityDescriptor = NULL; 
    if (!CreatePipe(&pr3287_stderr_rd, &pr3287_stderr_wr, &sa, 0)) {
	popup_an_error("CreatePipe() failed: %s",
		win32_strerror(GetLastError()));
	success = false;
	goto done;
    }
    if (!SetHandleInformation(pr3287_stderr_rd, HANDLE_FLAG_INHERIT, 0)) {
	popup_an_error("SetHandleInformation() failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(pr3287_stderr_rd);
	pr3287_stderr_rd = NULL;
	CloseHandle(pr3287_stderr_wr);
	pr3287_stderr_wr = NULL;
	success = false;
	goto done;
    }
    mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    if (!SetNamedPipeHandleState(pr3287_stderr_rd, &mode, NULL, NULL)) {
	popup_an_error("SetNamedPipeHandleState() failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(pr3287_stderr_rd);
	pr3287_stderr_rd = NULL;
	CloseHandle(pr3287_stderr_wr);
	pr3287_stderr_wr = NULL;
	success = false;
	goto done;
    }

    memset(&si, '\0', sizeof(si));
    si.cb = sizeof(pi);
    si.hStdError = pr3287_stderr_wr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    memset(&pi, '\0', sizeof(pi));
    if (!CreateProcess(NULL, cp_cmdline, NULL, NULL, TRUE, DETACHED_PROCESS,
		NULL, NULL, &si, &pi)) {
	popup_an_error("CreateProcess() for printer session failed: %s",
		win32_strerror(GetLastError()));
	CloseHandle(pr3287_stderr_rd);
	pr3287_stderr_rd = NULL;
	CloseHandle(pr3287_stderr_wr);
	pr3287_stderr_wr = NULL;
	success = false;
    } else {
	pr3287_handle = pi.hProcess;
	CloseHandle(pi.hThread);
	pr3287_id = AddInput(pr3287_handle, pr3287_reaped);
    }

done:
#endif /*]*/

    Free(cmd_text);

    /* Tell everyone else. */
    if (success) {
	pr3287_state = PRS_RUNNING;
	Replace(pr3287_running_lu, associated? NewString("."): NewString(lu));
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
	    strncpy(p->buf + p->count, exitmsg, space);
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
    assert(pr3287_state >= PRS_RUNNING);

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
    assert(pr3287_state == PRS_RUNNING);
    pr3287_sync = accept(pr3287_ls, (struct sockaddr *)&sin, &len);
    if (pr3287_sync == INVALID_SOCKET) {
	popup_a_sockerr("accept(printer sync)");
    } else {
	vtrace("Accepted sync connection from printer.\n");

#if !defined(_WIN32) /*[*/
	fcntl(pr3287_sync, F_SETFD, 1);
#endif /*]*/
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
static void
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

    if (pr3287_state == PRS_NONE) {
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
    if (pr3287_state == PRS_RUNNING) {
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
	char *stderr_text;

	CloseHandle(pr3287_handle);
	pr3287_handle = NULL;
	CloseHandle(pr3287_stderr_wr);
	pr3287_stderr_wr = NULL;

	stderr_text = read_pr3287_errors();
	CloseHandle(pr3287_stderr_rd);
	pr3287_stderr_rd = NULL;

	if (pr3287_state == PRS_RUNNING) {
	    popup_printer_output(true, NULL,
		    "%s%sPrinter process exited with status 0x%lx",
		    (stderr_text != NULL)? stderr_text: "",
		    (stderr_text != NULL)? "\n": "",
		    (long)exit_code);
	} else if (stderr_text != NULL) {
	    popup_printer_output(true, NULL, "%s", stderr_text);
	}
	if (stderr_text != NULL) {
	    Free(stderr_text);
	}
    } else {
	/* It is still running. */
	return;
    }
#endif /*]*/

    vtrace("Printer session exited.\n");

    /* Stop any pending printer kill request. */
    if (pr3287_state == PRS_SHUTDOWN) {
	assert(pr3287_kill_id != NULL_IOID);
	RemoveTimeOut(pr3287_kill_id);
	pr3287_kill_id = NULL_IOID;
	pr3287_state = PRS_NONE;
    }

    /* No need for sync input any more. */
    if (pr3287_sync_id != NULL_IOID) {
	pr3287_stop_sync();
    }

    /* (Try to) display pr3287's stderr. */
    if (pr3287_stderr.count > 0) {
	popup_an_error("%.*s", pr3287_stderr.count, pr3287_stderr.buf);
    } else {
	char buf[1024];
	ssize_t nr;

	nr = read(pr3287_stderr.fd, buf, sizeof(buf));
	if (nr > 0) {
	    popup_an_error("%.*s", (int)nr, buf);
	}
    }

    /* Clean up I/O. */
    pr3287_cleanup_io();

    pr3287_state = PRS_NONE;

    /* Propagate the state. */
    st_changed(ST_PRINTER, false);

    /*
     * If there is a pending request to start the printer, set a timeout to
     * start it.
     */
    if (pr3287_delay_lu != NULL) {
	pr3287_state = PRS_DELAY;
	pr3287_delay_id = AddTimeOut(get_printer_delay_ms(), delayed_start);
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
    kill(-pr3287_pid, SIGTERM);
#endif /*]*/

    pr3287_kill_id = NULL_IOID;
    pr3287_state = PRS_TERMINATING;
}


/* Close the printer session. */
void
pr3287_session_stop(void)
{
    switch (pr3287_state) {
    case PRS_DELAY:
	vtrace("Canceling delayed printer session start.\n");
	assert(pr3287_delay_id != NULL_IOID);
	RemoveTimeOut(pr3287_delay_id);
	pr3287_delay_id = NULL_IOID;
	assert(pr3287_delay_lu != NULL);
	Free(pr3287_delay_lu);
	pr3287_delay_lu = NULL;
	break;
    case PRS_RUNNING:
	/* Run through the logic below. */
	break;
    default:
	/* Nothing interesting to do. */
	return;
    }

    vtrace("Stopping printer session.\n");

    pr3287_cleanup_io();

    /* Set a timeout to terminate it not so gracefully. */
    pr3287_state = PRS_SHUTDOWN;
    pr3287_kill_id = AddTimeOut(PRINTER_KILL_MS, pr3287_kill);
}

/* The emulator is exiting.  Make sure the printer session is cleaned up. */
static void
pr3287_exiting(bool b _is_unused)
{
    if (pr3287_state >= PRS_RUNNING && pr3287_state < PRS_TERMINATING) {
	pr3287_kill(NULL_IOID);
    }
}

/* Return the current printer LU. */
static char *
pr3287_saved_lu(void)
{
    char *current = appres.interactive.printer_lu;

    return (current != NULL && !*current)? NULL: current;
}

/* Start a pr3287 session. */
static void
pr3287_connected(void)
{
    char *pr3287_lu = pr3287_saved_lu();

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
    } else if (!IN_E && pr3287_associated && pr3287_session_running()) {
	/* Stop an automatic associated printer. */
	pr3287_session_stop();
    }
}

/* Cancel any running or pending pr3287 session. */
static void
pr3287_disconnected(void)
{
    if (pr3287_session_running()) {
	/*
	 * We're no longer in 3270 mode, so we can no longer have a
	 * printer session.  This may cause some fireworks if there is
	 * a print job pending, so some sort of awful timeout may be
	 * needed.
	 */
	pr3287_session_stop();
    } else {
	/*
	 * Forget state associated with printer start-up.
	 */
	if (pr3287_state == PRS_DELAY) {
	    pr3287_state = PRS_NONE;
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

/* Host connect/disconnect/3270-mode event. */
static void
pr3287_host_connect(bool connected _is_unused)
{
    if (IN_3270) {
	pr3287_connected();
    } else {
	pr3287_disconnected();
    }
}

bool
pr3287_session_running(void)
{
    return (pr3287_state == PRS_RUNNING);
}

/*
 * Extended toggle for pr3287 Logical Unit.
 */
static toggle_upcall_ret_t
pr3287_toggle_lu(const char *name, const char *value, unsigned flags, ia_t ia)
{
    char *current = pr3287_saved_lu();

    if (!*value) {
	value = NULL;
    }
    if ((current == NULL && value == NULL) || (current != NULL && value != NULL && !strcmp(current, value))) {
	/* No change. */
	return TU_SUCCESS;
    }

    /* Save the new value. */
    Replace(appres.interactive.printer_lu,
	    (value != NULL)? NewString(value): NULL);

    /* Stop the current session. */
    pr3287_disconnected();

    /* Start a new session. */
    if (value != NULL && IN_3270) {
	pr3287_connected();
    }

    return TU_SUCCESS;
}

#if defined(_WIN32) /*[*/
/*
 * Extended toggle for pr3287 printer name.
 */
static toggle_upcall_ret_t
pr3287_toggle_name(const char *name, const char *value, unsigned flags, ia_t ia)
{
    char *current = get_resource(ResPrinterName);

    if (!*value) {
	value = NULL;
    }
    if ((current == NULL && value == NULL) ||
	    (current != NULL && value != NULL && !strcmp(current, value))) {
	/* No change. */
	return TU_SUCCESS;
    }

    /* Save the new value. */
    add_resource(ResPrinterName, value);

    /* Stop the current session. */
    pr3287_disconnected();

    /* Start a new session. */
    if (value != NULL && IN_3270) {
	pr3287_connected();
    }

    return TU_SUCCESS;
}

/*
 * Extended toggle for pr3287 printer code page.
 */
static toggle_upcall_ret_t
pr3287_toggle_codepage(const char *name, const char *value, unsigned flags, ia_t ia)
{
    char *current = get_resource(ResPrinterCodepage);

    if (!*value) {
	value = NULL;
    }
    if ((current == NULL && value == NULL) ||
	    (current != NULL && value != NULL && !strcmp(current, value))) {
	/* No change. */
	return TU_SUCCESS;
    }

    /* Save the new value. */
    add_resource(ResPrinterCodepage, value);

    /* Stop the current session. */
    pr3287_disconnected();

    /* Start a new session. */
    if (value != NULL && IN_3270) {
	pr3287_connected();
    }

    return TU_SUCCESS;
}
#endif /*]*/

/*
 * Extended toggle for pr3287 printer options.
 */
static toggle_upcall_ret_t
pr3287_toggle_opts(const char *name, const char *value, unsigned flags, ia_t ia)
{
    char *current = get_resource(ResPrinterOptions);

    if (!*value) {
	value = NULL;
    }
    if ((current == NULL && value == NULL) || (current != NULL && value != NULL && !strcmp(current, value))) {
	/* No change. */
	return TU_SUCCESS;
    }

    /* Save the new value. */
    add_resource(ResPrinterOptions, NewString(value));

    /* Stop the current session. */
    pr3287_disconnected();

    /* Start a new session. */
    if (value != NULL && IN_3270) {
	pr3287_connected();
    }

    return TU_SUCCESS;
}

/* Return the running printer LU. */
const char *
pr3287_session_lu(void)
{
    if (!pr3287_session_running()) {
	return NULL;
    }

    return pr3287_running_lu;
}
