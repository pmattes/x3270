# Copyright (c) 2007-2009, Paul Mattes.
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
