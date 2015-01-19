# Copyright (c) 2007-2015, Paul Mattes.
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

# Makefile for wpr3287

# Figure out if they've installed MinGW OpenSSL.
SSLDIR = /usr/local/OpenSSL-Win32
HAVE_OPENSSL = $(shell [ -d $(SSLDIR) ] && echo yes)
ifeq ($(HAVE_OPENSSL),yes)
ifndef NO_SSL
SSLCPP = -DHAVE_LIBSSL=1 -I$(SSLDIR)/include
SSLLIB = -lwsock32 -ladvapi32 -lgdi32 -luser32
else
OSUFFIX = -insecure
endif
endif

# Set command paths based on whether compiling natively on Cygwin, or
# cross-compiling on Linux.
OS = $(shell uname -o)
ifeq ($(OS),Cygwin)
CC = i686-pc-mingw32-gcc
WINDRES = i686-pc-mingw32-windres
else
CC = i586-mingw32msvc-gcc
WINDRES = i586-mingw32msvc-windres
endif

XCPPFLAGS = -D_WIN32 -DX3270_DBCS=1 -D_WIN32_WINNT=0x500 -D_WIN32_IE=0x0500 -DWINVER=0x500 -I. -I../include
CFLAGS = -g -Wall -Werror $(XCPPFLAGS) $(SSLCPP) $(CDEBUGFLAGS)

OBJ = o
include pr3287_files.mk
include wpr3287_files.mk
SRCS = $(PR3287_SOURCES) $(WPR3287_SOURCES)
VOBJS = $(PR3287_OBJECTS) $(WPR3287_OBJECTS)

OBJECTS = $(VOBJS) version.o
LIBS = $(SSLLIB) -lws2_32 -lwinspool

all: wpr3287.exe

XVERSION = xversion.c
version.o: $(VOBJS) version.txt mkversion.sh
	$(RM) $(XVERSION)
	@chmod +x mkversion.sh version.txt
	sh ./mkversion.sh wpr3287 >$(XVERSION)
	$(CC) $(CFLAGS) -c -o $@ $(XVERSION)
	$(RM) $(XVERSION)

wpr3287res.o: wpr3287.rc pr3287.ico
	$(WINDRES) -i wpr3287.rc -o wpr3287res.o

wpr3287.exe: $(OBJECTS)
	$(CC) -o wpr3287$(OSUFFIX).exe $(CFLAGS) $(OBJECTS) $(LIBS)

clean:
	rm -f *.o dependencies-

clobber: clean
	rm -f wpr3287.exe

depend:
	mingwmakedep -C $(CC) $(XCPPFLAGS) $(SRCS) >dependencies-
	mv dependencies- dependencies

ifeq ($(wildcard dependencies),dependencies)
include dependencies
endif

