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

# nmake Makefile for ws3270

# Set command paths.
MKFB = mkfb.exe
CC = cl /nologo
RC = rc /nologo
RM = erase

# This makefile assumes that you have OpenSSL headers in c:\OpenSSL-Win32.
# If they are somewhere else, change SSLDIR below.
SSLDIR = C:\OpenSSL-Win32
!IF EXISTS($(SSLDIR))
SSLDEF = /DHAVE_LIBSSL=1 /I$(SSLDIR)\include
SSLLIB = gdi32.lib
!ENDIF

XCPPFLAGS = /D_WIN32 /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500 /D_CRT_SECURE_NO_DEPRECATE /DWIN32_LEAN_AND_MEAN /W3 /WX $(SSLDEF) -I. -I..\include
CFLAGS = $(EXTRA_FLAGS) $(XCPPFLAGS)

OBJ = obj
!INCLUDE common_files.mk
!INCLUDE ws3270_files.mk

VOBJS = $(COMMON_OBJECTS) $(HTTPD_OBJECTS) $(WS3270_OBJECTS) ws3270.RES \
	fallbacks.obj

OBJECTS = $(VOBJS) version.obj
LIBS = ..\lib\libw3270.lib ..\lib\libw3270stubs.lib $(SSLLIB) ws2_32.lib \
       advapi32.lib user32.lib shell32.lib comdlg32.lib gdi32.lib \
       winspool.lib

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

ws3270.exe: $(OBJECTS) Msc\Makefile ..\lib\libw3270.lib ..\lib\libw3270stubs.lib
	$(CC) /Fews3270.exe $(CFLAGS) $(OBJECTS) $(LIBS)

..\lib\libw3270.lib:
	cd ..\lib\w3270 & $(MAKE) /nologo /fMsc\Makefile
..\lib\libw3270stubs.lib:
	cd ..\lib\w3270stubs & $(MAKE) /nologo /fMsc\Makefile

wversion.obj: version.txt mkversion.exe
	mkversion.exe -w
	$(CC) $(CFLAGS) /c wversion.c

clean:
	erase *.obj *.RES *.exp *.lib $(MKFB) fallbacks.c mkversion.exe version.c wversion.c

clobber: clean
	erase $(PROGS)
