# Copyright (c) 1999-2009, 2013-2015 Paul Mattes.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the names of Paul Mattes nor the names of his contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Makefile for 3270 emulation stubs library (Windows)

LIB3270 = ../libw3270stubs.a

# Figure out if they've installed MinGW OpenSSL.
SSLDIR = /usr/local/OpenSSL-Win32
HAVE_OPENSSL = $(shell [ -d $(SSLDIR) ] && echo yes)
ifeq ($(HAVE_OPENSSL),yes)
ifndef NO_SSL
SSLCPP = -DHAVE_LIBSSL=1 -I$(SSLDIR)/include
endif
endif

# Set command paths based on whether compiling natively on Cygwin, or
# cross-compiling on Linux.
OS = $(shell uname -o)
ifeq ($(OS),Cygwin)
CC = i686-pc-mingw32-gcc
AR = i686-pc-mingw32-ar
NATIVECC = gcc
WINDRES = i686-pc-mingw32-windres
else
CC = i586-mingw32msvc-gcc
AR = i586-mingw32msvc-ar
NATIVECC = gcc
WINDRES = i586-mingw32msvc-windres
endif

XCPPFLAGS = -D_WIN32 -D_WIN32_WINNT=0x0500 -D_WIN32_IE=0x0500 -DWINVER=0x500
CFLAGS = $(EXTRA_FLAGS) -g -Wall -Werror $(XCPPFLAGS) -I. $(SSLCPP)

all:: $(LIB3270)

OBJ = o
include lib3270stubs_files.mk
include libw3270stubs_files.mk
SRCS = $(LIB3270STUBS_SOURCES) $(LIBW3270STUBS_SOURCES)
OBJS = $(LIB3270STUBS_OBJECTS) $(LIBW3270STUBS_OBJECTS)

$(LIB3270): $(OBJS)
	$(AR) crs $@ $(OBJS)

clean:
	$(RM) *.o

clobber: clean
	$(RM) $(LIB3270)

depend: $(SRCS)
	mingwmakedep -C $(CC) $(XCPPFLAGS) -I. $(SRCS) >dependencies-
	mv dependencies- dependencies

ifeq ($(wildcard dependencies),dependencies)
include dependencies
endif
