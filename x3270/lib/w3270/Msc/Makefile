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

# nmake Makefile for libw3270

TARGET = ..\libw3270.lib

# Set command paths.
CC = cl /nologo
RM = erase
LIB = lib /nologo

# This makefile assumes that you have OpenSSL headers in c:\OpenSSL-Win32.
# If they are somewhere else, change SSLDIR below.
SSLDIR = C:\OpenSSL-Win32
!IF EXISTS($(SSLDIR))
SSLDEF = /DHAVE_LIBSSL=1 /I$(SSLDIR)\include
!ENDIF

XCPPFLAGS = /D_WIN32 /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500 /DWINVER=0x500 /D_CRT_SECURE_NO_DEPRECATE /W3 /WX $(SSLDEF)
CFLAGS = $(EXTRA_FLAGS) $(XCPPFLAGS) -I. -I..\..\include

OBJECTS = Malloc.obj asprintf.obj lazya.obj varbuf.obj

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LIB) $(OBJECTS) -OUT:$@

clean:
	erase *.obj

clobber: clean
	erase $(TARGET)
