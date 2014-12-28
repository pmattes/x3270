# Copyright (c) 2007-2014, Paul Mattes.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Paul Mattes nor his contributors may be used
#       to endorse or promote products derived from this software without
#       specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# nmake Makefile for ws3270

# Set command paths.
MKFB = mkfb.exe
CC = cl /nologo
RC = rc
RM = erase

# This makefile assumes that you have OpenSSL headers in c:\OpenSSL-Win32.
# If they are somewhere else, change SSLDIR below.
SSLDIR = C:\OpenSSL-Win32
!IF EXISTS($(SSLDIR))
SSLDEF = /DHAVE_LIBSSL=1 /I$(SSLDIR)\include
SSLLIB = gdi32.lib
!ENDIF

XCPPFLAGS = /D_WIN32 /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500 /D_CRT_SECURE_NO_DEPRECATE /DWIN32_LEAN_AND_MEAN /W3 /WX $(SSLDEF)
CFLAGS = $(EXTRA_FLAGS) $(XCPPFLAGS) -I.

VOBJS = XtGlue.obj actions.obj apl.obj asprintf.obj bind-opt.obj charset.obj \
	ctlr.obj fallbacks.obj favicon.obj fprint_screen.obj ft.obj \
	ft_cut.obj ft_dft.obj glue.obj host.obj httpd-core.obj httpd-io.obj \
	httpd-nodes.obj idle.obj kybd.obj linemode.obj macros.obj nvt.obj \
	print.obj printer.obj proxy.obj readres.obj resolver.obj \
	resources.obj rpq.obj see.obj sf.obj s3270.obj snprintf.obj \
	ssl_dll.obj tables.obj telnet.obj toggles.obj trace.obj \
	unicode.obj unicode_dbcs.obj utf8.obj util.obj varbuf.obj w3misc.obj \
	windirs.obj winprint.obj winvers.obj ws3270.RES xio.obj

OBJECTS = $(VOBJS) version.obj
LIBS = $(SSLLIB) ws2_32.lib advapi32.lib user32.lib shell32.lib
SHRTLIBS = ole32.lib
WIZLIBS = ole32.lib winspool.lib advapi32.lib

PROGS = ws3270.exe
all: $(PROGS)

version.obj: $(VOBJS) version.txt mkversion.exe Msc\Makefile
	mkversion.exe
	$(CC) $(CFLAGS) /c version.c
mkversion.exe: Msc\mkversion.c
	$(CC) $(CFLAGS) /Fe$@ Msc\mkversion.c

fallbacks.c: $(MKFB) X3270.xad Msc\Makefile
	$(MKFB) -c X3270.xad $@
fallbacks.obj: fallbacks.c
	$(CC) $(CFLAGS) /c fallbacks.c

$(MKFB): mkfb.c Msc\Makefile
	$(CC) /Fe$(MKFB) $(XCPPFLAGS) mkfb.c

ws3270.RES: ws3270.rc ws3270.ico Msc\Makefile
	$(RC) ws3270.rc

ws3270.exe: $(OBJECTS) Msc\Makefile
	$(CC) /Fews3270.exe $(CFLAGS) $(OBJECTS) $(LIBS)

wversion.obj: version.txt mkversion.exe
	mkversion.exe -w
	$(CC) $(CFLAGS) /c wversion.c

clean:
	erase *.obj *.RES *.exp *.lib $(MKFB) fallbacks.c mkversion.exe version.c wversion.c

clobber: clean
	erase $(PROGS)

XtGlue.obj: XtGlue.c
	$(CC) $(CFLAGS) /c XtGlue.c
actions.obj: actions.c
	$(CC) $(CFLAGS) /c actions.c
nvt.obj: nvt.c
	$(CC) $(CFLAGS) /c nvt.c
apl.obj: apl.c
	$(CC) $(CFLAGS) /c apl.c
asprintf.obj: asprintf.c
	$(CC) $(CFLAGS) /c asprintf.c
bind-opt.obj: bind-opt.c
	$(CC) $(CFLAGS) /c bind-opt.c
charset.obj: charset.c
	$(CC) $(CFLAGS) /c charset.c
ctlr.obj: ctlr.c
	$(CC) $(CFLAGS) /c ctlr.c
favicon.obj: favicon.c
	$(CC) $(CFLAGS) /c favicon.c
fprint_screen.obj: fprint_screen.c
	$(CC) $(CFLAGS) /c fprint_screen.c
ft.obj: ft.c
	$(CC) $(CFLAGS) /c ft.c
ft_cut.obj: ft.c
	$(CC) $(CFLAGS) /c ft_cut.c
ft_dft.obj: ft_dft.c
	$(CC) $(CFLAGS) /c ft_dft.c
glue.obj: glue.c
	$(CC) $(CFLAGS) /c glue.c
host.obj: host.c
	$(CC) $(CFLAGS) /c host.c
httpd-core.obj: httpd-core.c
	$(CC) $(CFLAGS) /c httpd-core.c
httpd-io.obj: httpd-io.c
	$(CC) $(CFLAGS) /c httpd-io.c
httpd-nodes.obj: httpd-nodes.c
	$(CC) $(CFLAGS) /c httpd-nodes.c
idle.obj: idle.c
	$(CC) $(CFLAGS) /c idle.c
kybd.obj: kybd.c
	$(CC) $(CFLAGS) /c kybd.c
linemode.obj: linemode.c
	$(CC) $(CFLAGS) /c linemode.c
macros.obj: macros.c
	$(CC) $(CFLAGS) /c macros.c
mkshort.obj: mkshort.c
	$(CC) $(CFLAGS) /c mkshort.c
print.obj: print.c
	$(CC) $(CFLAGS) /c print.c
printer.obj: printer.c
	$(CC) $(CFLAGS) /c printer.c
proxy.obj: proxy.c
	$(CC) $(CFLAGS) /c proxy.c
readres.obj: readres.c
	$(CC) $(CFLAGS) /c readres.c
resolver.obj: resolver.c
	$(CC) $(CFLAGS) /c resolver.c
resources.obj: resources.c
	$(CC) $(CFLAGS) /c resources.c
rpq.obj: rpq.c
	$(CC) $(CFLAGS) /c rpq.c
see.obj: see.c
	$(CC) $(CFLAGS) /c see.c
sf.obj: sf.c
	$(CC) $(CFLAGS) /c sf.c
shortcut.obj: shortcut.c
	$(CC) $(CFLAGS) /c shortcut.c
s3270.obj: s3270.c
	$(CC) $(CFLAGS) /c s3270.c
snprintf.obj: snprintf.c
	$(CC) $(CFLAGS) /c snprintf.c
ssl_dll.obj: ssl_dll.c
	$(CC) $(CFLAGS) /c ssl_dll.c
tables.obj: tables.c
	$(CC) $(CFLAGS) /c tables.c
telnet.obj: telnet.c
	$(CC) $(CFLAGS) /c telnet.c
toggles.obj: toggles.c
	$(CC) $(CFLAGS) /c toggles.c
trace.obj: trace.c
	$(CC) $(CFLAGS) /c trace.c
unicode.obj: unicode.c
	$(CC) $(CFLAGS) /c unicode.c
unicode_dbcs.obj: unicode_dbcs.c
	$(CC) $(CFLAGS) /c unicode_dbcs.c
utf8.obj: utf8.c
	$(CC) $(CFLAGS) /c utf8.c
util.obj: util.c
	$(CC) $(CFLAGS) /c util.c
varbuf.obj: varbuf.c
	$(CC) $(CFLAGS) /c varbuf.c
winvers.obj: winvers.c
	$(CC) $(CFLAGS) /c winvers.c
windirs.obj: windirs.c
	$(CC) $(CFLAGS) /c windirs.c
w3misc.obj: w3misc.c
	$(CC) $(CFLAGS) /c w3misc.c
winprint.obj: winprint.c
	$(CC) $(CFLAGS) /c winprint.c
wizard.obj: wizard.c
	$(CC) $(CFLAGS) /c wizard.c
xio.obj: xio.c
	$(CC) $(CFLAGS) /c xio.c
