# Copyright (c) 2007-2009, 2013-2014 Paul Mattes.
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

# Makefile for ws3270

# Figure out if they've installed MinGW OpenSSL.
SSLDIR = /usr/local/OpenSSL-Win32
HAVE_OPENSSL = $(shell [ -d $(SSLDIR) ] && echo yes)
ifeq ($(HAVE_OPENSSL),yes)
ifndef NO_SSL
SSLCPP = -DHAVE_LIBSSL=1 -I$(SSLDIR)/include
#SSLLIB = -L$(SSLDIR)/lib/MinGW $(SSLDIR)/lib/MinGW/ssleay32.a -leay32 -lwsock32 -ladvapi32 -lgdi32 -luser32
SSLLIB = -lwsock32 -ladvapi32 -lgdi32 -luser32
else
OSUFFIX = -insecure
endif
endif

# Set command paths and mkfb (which has to run locally) based on whether
# compiling natively on Cygwin, or cross-compiling on Linux.
OS = $(shell uname -o)
ifeq ($(OS),Cygwin)
MKFB = mkfb.exe
CC = i686-pc-mingw32-gcc
NATIVECC = gcc
WINDRES = i686-pc-mingw32-windres
else
MKFB = mkfb
CC = i586-mingw32msvc-gcc
NATIVECC = gcc
WINDRES = i586-mingw32msvc-windres
endif

XCPPFLAGS = -D_WIN32 -D_WIN32_WINNT=0x0500 -D_WIN32_IE=0x0500 -DWIN32_LEAN_AND_MEAN
CFLAGS = $(EXTRA_FLAGS) -g -Wall -Werror $(XCPPFLAGS) -I. $(SSLCPP)

SRCS = XtGlue.c actions.c apl.c asprintf.c bind-opt.c charset.c ctlr.c \
	fallbacks.c favicon.c fprint_screen.c ft.c ft_cut.c ft_dft.c glue.c \
	host.c httpd-core.c httpd-io.c httpd-nodes.c idle.c kybd.c linemode.c \
	macros.c nvt.c print.c printer.c proxy.c readres.c resolver.c \
	resources.c rpq.c see.c sf.c smain.c snprintf.c ssl_dll.c strtok_r.c \
	tables.c telnet.c toggles.c trace_ds.c unicode.c unicode_dbcs.c \
	utf8.c util.c varbuf.c w3misc.c windirs.c winprint.c winvers.c xio.c

VOBJS = XtGlue.o actions.o apl.o asprintf.o bind-opt.o charset.o ctlr.o \
	fallbacks.o favicon.o fprint_screen.o ft.o ft_cut.o ft_dft.o glue.o \
	host.o httpd-core.o httpd-io.o httpd-nodes.o idle.o kybd.o linemode.o \
	macros.o nvt.o print.o printer.o proxy.o readres.o resolver.o \
	resources.o rpq.o see.o sf.o smain.o snprintf.o ssl_dll.o strtok_r.o \
	tables.o telnet.o toggles.o trace_ds.o unicode.o unicode_dbcs.o \
	utf8.o util.o varbuf.o w3misc.o windirs.o winprint.o winvers.o \
	ws3270res.o xio.o

OBJECTS = $(VOBJS) version.o
LIBS = $(SSLLIB) -lws2_32
SHRTLIBS = -lole32 -luuid
DLLFLAGS = $(EXTRA_FLAGS) -mno-cygwin -shared -Wl,--export-all-symbols -Wl,--enable-auto-import

PROGS = ws3270.exe
all: $(PROGS)

XVERSION = xversion.c
version.o: $(VOBJS) version.txt mkversion.sh Makefile
	$(RM) $(XVERSION)
	@chmod +x mkversion.sh version.txt
	sh ./mkversion.sh ws3270 >$(XVERSION)
	$(CC) $(CFLAGS) -c -o $@ $(XVERSION)
	$(RM) $(XVERSION)

fallbacks.c: $(MKFB) X3270.xad Makefile
	$(RM) $@
	./$(MKFB) -c X3270.xad $@

$(MKFB): mkfb.c Makefile
	$(NATIVECC) -g -o $(MKFB) -DFOR_WIN32 mkfb.c

ws3270res.o: ws3270.rc ws3270.ico Makefile
	$(WINDRES) -i ws3270.rc -o ws3270res.o

ws3270.exe: $(OBJECTS) Makefile
	$(CC) -o ws3270$(OSUFFIX).exe $(CFLAGS) $(OBJECTS) $(LIBS)

wversion.o: version.txt mkwversion.sh
	@chmod +x mkwversion.sh version.txt
	sh ./mkwversion.sh $(CC)

clean:
	rm -f *.o mkfb mkfb.exe fallbacks.c

clobber: clean
	rm -f $(PROGS)

depend: $(SRCS)
	mingwmakedep -C $(CC) $(XCPPFLAGS) -I. $(SRCS) >dependencies

ifeq ($(wildcard dependencies),dependencies)
include dependencies
endif

