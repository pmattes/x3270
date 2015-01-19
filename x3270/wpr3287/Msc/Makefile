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

# nmake Makefile for wpr3287

# Set command paths.
CC = cl /nologo
RC = rc
RM = erase

# This makefile assumes that you have OpenSSL headers in c:\OpenSSL-Win32.
# If they are somewhere else, change SSLDIR below.
SSLDIR = C:\OpenSSL-Win32
!IF EXISTS($(SSLDIR))
SSLDEF = /DHAVE_LIBSSL=1 /I$(SSLDIR)\include
SSLLIB = gdi32.lib user32.lib
!ENDIF

XCPPFLAGS = /D_WIN32 /DX3270_DBCS=1 /D_CRT_SECURE_NO_DEPRECATE /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500 /W3 /WX $(SSLDEF) -I. -I..\include
CFLAGS = $(XCPPFLAGS)

OBJ = obj
!INCLUDE pr3287_files.mk
!INCLUDE wpr3287_files.mk
VOBJS = $(PR3287_OBJECTS) $(WPR3287_OBJECTS)

OBJECTS = $(VOBJS) version.obj
LIBS = $(SSLLIB) ws2_32.lib winspool.lib advapi32.lib shell32.lib

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
