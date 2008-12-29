# Copyright 2007-2008 by Paul Mattes.
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for any purpose and without fee is hereby granted,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation.
#
# wpr3287 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.

# nmake Makefile for wpr3287

# Set command paths.
CC = cl /nologo
RC = rc
RM = erase

# To build with a statically-linked OpenSSL library, uncomment the following
#  three lines (substituting your OpenSSL install location in the first).
#SSLDIR = C:\where\openssl\is\installed
#SSLDEF = /DHAVE_LIBSSL=1 /I$(SSLDIR)\include
#SSLLIB = $(SSLDIR)\lib\ssleay32.lib $(SSLDIR)\lib\libeay32.lib gdi32.lib user32.lib

XCPPFLAGS = /D_WIN32 /DPR3287 /DWPR3287 /DX3270_DBCS=1 /D_CRT_SECURE_NO_DEPRECATE /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500 $(SSLDEF)
CFLAGS = $(XCPPFLAGS) -I.
VOBJS = charset.obj ctlr.obj pr3287.obj proxy.obj resolver.obj see.obj sf.obj \
	tables.obj telnet.obj trace_ds.obj utf8.obj wpr3287.RES ws.obj \
	w3misc.obj windirs.obj winvers.obj unicode.obj unicode_dbcs.obj
OBJECTS = $(VOBJS) version.obj
LIBS = $(SSLLIB) ws2_32.lib winspool.lib advapi32.lib

all: wpr3287.exe

version.obj: $(VOBJS) version.txt mkversion.exe Msc\Makefile
	mkversion.exe wpr3287
	$(CC) $(CFLAGS) /c version.c
mkversion.exe: Msc\mkversion.c
	$(CC) $(CFLAGS) /Fe$@ Msc\mkversion.c

wpr3287.RES: wpr3287.rc pr3287.ico
	$(RC) wpr3287.rc

wpr3287.exe: $(OBJECTS)
	$(CC) /Fewpr3287.exe $(CFLAGS) $(OBJECTS) $(LIBS)

clean:
	$(RM) *.obj *.RES mkversion.exe version.c

clobber: clean
	$(RM) wpr3287.exe

charset.obj: charset.c
	$(CC) $(CFLAGS) /c charset.c
ctlr.obj: ctlr.c
	$(CC) $(CFLAGS) /c ctlr.c
pr3287.obj: pr3287.c
	$(CC) $(CFLAGS) /c pr3287.c
proxy.obj: proxy.c
	$(CC) $(CFLAGS) /c proxy.c
resolver.obj: resolver.c
	$(CC) $(CFLAGS) /c resolver.c
see.obj: see.c
	$(CC) $(CFLAGS) /c see.c
sf.obj: sf.c
	$(CC) $(CFLAGS) /c sf.c
tables.obj: tables.c
	$(CC) $(CFLAGS) /c tables.c
telnet.obj: telnet.c
	$(CC) $(CFLAGS) /c telnet.c
trace_ds.obj: trace_ds.c
	$(CC) $(CFLAGS) /c trace_ds.c
unicode.obj: unicode.c
	$(CC) $(CFLAGS) /c unicode.c
unicode_dbcs.obj: unicode_dbcs.c
	$(CC) $(CFLAGS) /c unicode_dbcs.c
utf8.obj: utf8.c
	$(CC) $(CFLAGS) /c utf8.c
w3misc.obj: w3misc.c
	$(CC) $(CFLAGS) /c w3misc.c
windirs.obj: windirs.c
	$(CC) $(CFLAGS) /c windirs.c
winvers.obj: winvers.c
	$(CC) $(CFLAGS) /c winvers.c
ws.obj: ws.c
	$(CC) $(CFLAGS) /c ws.c
