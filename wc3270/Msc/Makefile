# Copyright 2007-2008 by Paul Mattes.
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for any purpose and without fee is hereby granted,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation.
#
# wc3270 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.

# nmake Makefile for wc3270

# Set command paths.
MKFB = mkfb.exe
CC = cl /nologo
RC = rc
RM = erase

# To build with a statically-linked OpenSSL library, uncomment the following
#  three lines (substituting your OpenSSL install location in the first).
#SSLDIR = C:\where\openssl\is\installed
#SSLDEF = /DHAVE_LIBSSL=1 /I$(SSLDIR)\include
#SSLLIB = $(SSLDIR)\lib\ssleay32.lib $(SSLDIR)\lib\libeay32.lib gdi32.lib

XCPPFLAGS = /D_WIN32 /DWC3270 /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500 /D_CRT_SECURE_NO_DEPRECATE $(SSLDEF)
CFLAGS = $(EXTRA_FLAGS) $(XCPPFLAGS) -I. $(SSLCPP)
SRCS = XtGlue.c actions.c ansi.c apl.c c3270.c charset.c ctlr.c \
	ft.c ft_cut.c ft_dft.c glue.c help.c host.c icmd.c idle.c kybd.c \
	macros.c print.c printer.c proxy.c resources.c rpq.c screen.c see.c \
	sf.c tables.c telnet.c toggles.c trace_ds.c unicode.c utf8.c util.c \
	xio.c fallbacks.c keymap.c w3misc.c winvers.c windirs.c resolver.c
VOBJS = XtGlue.obj actions.obj ansi.obj apl.obj c3270.obj charset.obj \
	ctlr.obj fallbacks.obj ft.obj ft_cut.obj ft_dft.obj glue.obj help.obj \
	host.obj icmd.obj idle.obj keymap.obj kybd.obj macros.obj print.obj \
	printer.obj proxy.obj resolver.obj resources.obj rpq.obj screen.obj \
	see.obj sf.obj tables.obj telnet.obj toggles.obj trace_ds.obj \
	unicode.obj utf8.obj util.obj xio.obj w3misc.obj winvers.obj \
	windirs.obj wc3270.RES
OBJECTS = $(VOBJS) version.obj
LIBS = $(SSLLIB) ws2_32.lib advapi32.lib user32.lib
SHRTLIBS = ole32.lib
WIZLIBS = ole32.lib winspool.lib advapi32.lib

PROGS = wc3270.exe mkshort.exe wc3270wiz.exe catf.exe w3n4.dll w3n46.dll shf.dll ead3270.exe
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
	$(CC) /Fe$(MKFB) /D_WIN32 /D_CRT_SECURE_NO_DEPRECATE mkfb.c

wc3270.RES: wc3270.rc wc3270.ico Msc\Makefile
	$(RC) wc3270.rc

wc3270.exe: $(OBJECTS) Msc\Makefile
	$(CC) /Fewc3270.exe $(CFLAGS) $(OBJECTS) $(LIBS)

mkshort.exe: mkshort.obj shortcut.obj winvers.obj Msc\Makefile
	$(CC) /Femkshort.exe $(CFLAGS) \
		mkshort.obj shortcut.obj winvers.obj $(SHRTLIBS)

wversion.obj: version.txt mkversion.exe
	mkversion.exe -w
	$(CC) $(CFLAGS) /c wversion.c

wc3270wiz.exe: wizard.obj wc3270.RES wversion.obj shortcut.obj winvers.obj windirs.obj
	$(CC) /Fewc3270wiz.exe $(CFLAGS) \
		wizard.obj wc3270.RES wversion.obj shortcut.obj winvers.obj \
		windirs.obj $(WIZLIBS)

catf.exe: catf.c
	$(CC) /Fecatf.exe $(CFLAGS) catf.c

w3n4.dll: w3n4.obj w3misc.obj
	$(CC) /LD /Few3n4.dll w3n4.obj ws2_32.lib /link /export:dresolve_host_and_port

w3n46.dll: w3n46.obj w3misc.obj
	$(CC) /LD /Few3n46.dll w3n46.obj ws2_32.lib /link /export:dresolve_host_and_port

w3n4.obj: resolver.c
	$(CC) $(CFLAGS) /DW3N4 /c /Fow3n4.obj resolver.c

w3n46.obj: resolver.c
	$(CC) $(CFLAGS) /DW3N46  /D_WIN32_WINNT=0x0501 /D_WIN32_IE=0x0501 /c /Fow3n46.obj resolver.c

shf.dll: shellfolder.obj
	$(CC) /LD /Feshf.dll shellfolder.obj shell32.lib /link /export:get_shell_folders

shellfolder.obj: shellfolder.c
	$(CC) $(CFLAGS) /c /Foshellfolder.obj shellfolder.c

ead3270.exe: ead3270.obj windirs.obj
	$(CC) /Fe$@ $(CFLAGS) ead3270.obj windirs.obj advapi32.lib

ead3270.obj: ead3270.c
	$(CC) $(CFLAGS) /c ead3270.c

clean:
	erase *.obj *.RES *.exp *.lib $(MKFB) fallbacks.c mkversion.exe version.c wversion.c

clobber: clean
	erase $(PROGS)

XtGlue.obj: XtGlue.c
	$(CC) $(CFLAGS) /c XtGlue.c
actions.obj: actions.c
	$(CC) $(CFLAGS) /c actions.c
ansi.obj: ansi.c
	$(CC) $(CFLAGS) /c ansi.c
apl.obj: apl.c
	$(CC) $(CFLAGS) /c apl.c
c3270.obj: c3270.c
	$(CC) $(CFLAGS) /c c3270.c
charset.obj: charset.c
	$(CC) $(CFLAGS) /c charset.c
ctlr.obj: ctlr.c
	$(CC) $(CFLAGS) /c ctlr.c
ft.obj: ft.c
	$(CC) $(CFLAGS) /c ft.c
ft_cut.obj: ft.c
	$(CC) $(CFLAGS) /c ft_cut.c
ft_dft.obj: ft_dft.c
	$(CC) $(CFLAGS) /c ft_dft.c
glue.obj: glue.c
	$(CC) $(CFLAGS) /c glue.c
help.obj: help.c
	$(CC) $(CFLAGS) /c help.c
host.obj: host.c
	$(CC) $(CFLAGS) /c host.c
icmd.obj: icmd.c
	$(CC) $(CFLAGS) /c icmd.c
idle.obj: idle.c
	$(CC) $(CFLAGS) /c idle.c
keymap.obj: keymap.c
	$(CC) $(CFLAGS) /c keymap.c
kybd.obj: kybd.c
	$(CC) $(CFLAGS) /c kybd.c
macros.obj: macros.c
	$(CC) $(CFLAGS) /c macros.c
print.obj: print.c
	$(CC) $(CFLAGS) /c print.c
printer.obj: printer.c
	$(CC) $(CFLAGS) /c printer.c
proxy.obj: proxy.c
	$(CC) $(CFLAGS) /c proxy.c
resolver.obj: resolver.c
	$(CC) $(CFLAGS) /c resolver.c
resources.obj: resources.c
	$(CC) $(CFLAGS) /c resources.c
rpq.obj: rpq.c
	$(CC) $(CFLAGS) /c rpq.c
screen.obj: screen.c
	$(CC) $(CFLAGS) /c screen.c
see.obj: see.c
	$(CC) $(CFLAGS) /c see.c
sf.obj: sf.c
	$(CC) $(CFLAGS) /c sf.c
tables.obj: tables.c
	$(CC) $(CFLAGS) /c tables.c
telnet.obj: telnet.c
	$(CC) $(CFLAGS) /c telnet.c
toggles.obj: toggles.c
	$(CC) $(CFLAGS) /c toggles.c
trace_ds.obj: trace_ds.c
	$(CC) $(CFLAGS) /c trace_ds.c
unicode.obj: unicode.c
	$(CC) $(CFLAGS) /c unicode.c
utf8.obj: utf8.c
	$(CC) $(CFLAGS) /c utf8.c
util.obj: util.c
	$(CC) $(CFLAGS) /c util.c
xio.obj: xio.c
	$(CC) $(CFLAGS) /c xio.c
winvers.obj: winvers.c
	$(CC) $(CFLAGS) /c winvers.c
windirs.obj: windirs.c
	$(CC) $(CFLAGS) /c windirs.c
mkshort.obj: mkshort.c
	$(CC) $(CFLAGS) /c mkshort.c
shortcut.obj: shortcut.c
	$(CC) $(CFLAGS) /c shortcut.c
wizard.obj: wizard.c
	$(CC) $(CFLAGS) /c wizard.c
w3misc.obj: w3misc.c
	$(CC) $(CFLAGS) /c w3misc.c
