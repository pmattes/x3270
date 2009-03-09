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

# Makefile for wc3270

# Figure out if they've installed MinGW OpenSSL.
SSLDIR = /usr/local/mingw-openssl
HAVE_OPENSSL = $(shell [ -d $(SSLDIR) ] && echo yes)
ifeq ($(HAVE_OPENSSL),yes)
SSLCPP = -DHAVE_LIBSSL=1 -I$(SSLDIR)/include
SSLLIB = -L$(SSLDIR)/lib -lssl -lcrypto -lwsock32 -ladvapi32 -lgdi32 -luser32
endif

# Set command paths and mkfb (which has to run locally) based on whether
# compiling natively on Cygwin, or cross-compiling on Linux.
ifeq ($(CROSS),1)
MKFB = mkfb
CC = i586-mingw32msvc-gcc
NATIVECC = gcc
WINDRES = i586-mingw32msvc-windres
NO_CYGWIN =
else
MKFB = mkfb.exe
CC = gcc
NATIVECC = gcc
WINDRES = windres
NO_CYGWIN = -mno-cygwin
endif

XCPPFLAGS = -D_WIN32 -DWC3270 -D_WIN32_WINNT=0x0500 -D_WIN32_IE=0x0500 -DWINVER=0x500
CFLAGS = $(EXTRA_FLAGS) $(NO_CYGWIN) -g -Wall -Werror $(XCPPFLAGS) -I. $(SSLCPP)
SRCS = XtGlue.c actions.c ansi.c apl.c c3270.c charset.c ctlr.c \
	ft.c ft_cut.c ft_dft.c glue.c help.c host.c icmd.c idle.c kybd.c \
	macros.c print.c printer.c proxy.c readres.c resources.c rpq.c \
	screen.c see.c sf.c tables.c telnet.c toggles.c trace_ds.c unicode.c \
	unicode_dbcs.c utf8.c util.c xio.c fallbacks.c keymap.c w3misc.c \
	winvers.c windirs.c resolver.c
VOBJS = XtGlue.o actions.o ansi.o apl.o c3270.o charset.o ctlr.o \
	fallbacks.o ft.o ft_cut.o ft_dft.o glue.o help.o host.o icmd.o idle.o \
	keymap.o kybd.o macros.o print.o printer.o proxy.o readres.o \
	resolver.o resources.o rpq.o screen.o see.o sf.o tables.o telnet.o \
	toggles.o trace_ds.o unicode.o unicode_dbcs.o utf8.o util.o xio.o \
	w3misc.c winvers.o windirs.o wc3270res.o
OBJECTS = $(VOBJS) version.o
LIBS = $(SSLLIB) -lws2_32
SHRTLIBS = -lole32 -luuid
WIZLIBS = -lole32 -luuid -lwinspool
DLLFLAGS = $(EXTRA_FLAGS) -mno-cygwin -shared -Wl,--export-all-symbols -Wl,--enable-auto-import

PROGS = wc3270.exe mkshort.exe wc3270wiz.exe catf.exe w3n4.dll w3n46.dll shf.dll ead3270.exe
all: $(PROGS)

wc3270.exe : XCPPFLAGS += -DWIN32_LEAN_AND_MEAN

version.o: $(VOBJS) version.txt mkversion.sh Makefile
	@chmod +x mkversion.sh version.txt
	sh ./mkversion.sh $(CC) wc3270

fallbacks.c: $(MKFB) X3270.xad Makefile
	$(RM) $@
	./$(MKFB) -c X3270.xad $@

$(MKFB): mkfb.c Makefile
	$(NATIVECC) -g -o $(MKFB) -D_WIN32 mkfb.c

wc3270res.o: wc3270.rc wc3270.ico Makefile
	$(WINDRES) -i wc3270.rc -o wc3270res.o

wc3270.exe: $(OBJECTS) Makefile
	$(CC) -o wc3270.exe $(CFLAGS) $(OBJECTS) $(LIBS)

mkshort.exe: mkshort.o shortcut.o winvers.o Makefile
	$(CC) -o mkshort.exe $(CFLAGS) \
		mkshort.o shortcut.o winvers.o $(SHRTLIBS)

wversion.o: version.txt mkwversion.sh
	@chmod +x mkwversion.sh version.txt
	sh ./mkwversion.sh $(CC)

wc3270wiz.exe: wizard.o wc3270res.o wversion.o shortcut.o winvers.o windirs.o
	$(CC) -o wc3270wiz.exe $(CFLAGS) \
		wizard.o wc3270res.o wversion.o shortcut.o winvers.o \
		windirs.o $(WIZLIBS)

catf.exe: catf.c
	$(CC) -o catf.exe $(CFLAGS) catf.c

w3n4.dll: w3n4.o w3misc.o
	$(CC) $(DLLFLAGS) -o w3n4.dll w3n4.o -lws2_32

w3n46.dll: w3n46.o w3misc.o
	$(CC) $(DLLFLAGS) -o w3n46.dll w3n46.o -lws2_32

w3n4.o: resolver.c
	$(CC) $(CFLAGS) -DW3N4 -c -o w3n4.o resolver.c

w3n46.o: resolver.c
	$(CC) $(CFLAGS) -DW3N46 -c -o w3n46.o resolver.c

shf.dll: shellfolder.o
	$(CC) $(DLLFLAGS) -o shf.dll shellfolder.o

shellfolder.o: shellfolder.c
	$(CC) $(CFLAGS) -c -o shellfolder.o shellfolder.c

ead3270.exe: ead3270.o windirs.o
	$(CC) -o $@ $(CFLAGS) ead3270.o windirs.o

ead3270.o: ead3270.c
	$(CC) $(CFLAGS) -c ead3270.c

clean:
	rm -f *.o mkfb mkfb.exe fallbacks.c

clobber: clean
	rm -f $(PROGS)

depend:
	gccmakedep -fMakefile $(XCPPFLAGS) -s "# DO NOT DELETE" $(SRCS)

# -------------------------------------------------------------------------
# dependencies generated by makedepend

# DO NOT DELETE
