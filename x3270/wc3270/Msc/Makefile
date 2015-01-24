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

# nmake Makefile for wc3270

# Set command paths.
MKFB = mkfb.exe
MKKEYPAD = mkkeypad.exe
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

XCPPFLAGS = /D_WIN32 /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500 /DWINVER=0x500 /D_CRT_SECURE_NO_DEPRECATE /W3 /WX $(SSLDEF) -I. -I..\include
CFLAGS = $(EXTRA_FLAGS) $(XCPPFLAGS)

OBJ = obj
!INCLUDE common_files.mk
!INCLUDE wc3270_files.mk

VOBJS = $(COMMON_OBJECTS) $(HTTPD_OBJECTS) $(WC3270_OBJECTS) wc3270.RES \
	fallbacks.obj

OBJECTS = $(VOBJS) version.obj
WOBJECTS = ..\lib\libw3270.lib wizard.obj wc3270.RES wversion.obj \
	   shortcut.obj winvers.obj windirs.obj relink.obj
LIBS = ..\lib\libw3270.lib ..\lib\libw3270stubs.lib $(SSLLIB) ws2_32.lib \
       advapi32.lib user32.lib shell32.lib ole32.lib comdlg32.lib gdi32.lib \
       winspool.lib
SHRTLIBS = ole32.lib
WIZLIBS = ole32.lib winspool.lib advapi32.lib shell32.lib

PROGS = wc3270.exe mkshort.exe wc3270wiz.exe catf.exe ead3270.exe x3270if.exe
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
	$(CC) /Fe$(MKFB) $(XCPPFLAGS)  mkfb.c

compiled_keypad.h: keypad.full keypad.labels keypad.map keypad.outline keypad.callbacks $(MKKEYPAD)
	$(MKKEYPAD) >$@

$(MKKEYPAD): mkkeypad.c
	$(CC) /Fe$(MKKEYPAD) $(XCPPFLAGS)  mkkeypad.c

wc3270.RES: wc3270.rc wc3270.ico Msc\Makefile
	$(RC) wc3270.rc

wc3270.exe: $(OBJECTS) Msc\Makefile ..\lib\libw3270.lib ..\lib\libw3270stubs.lib
	$(CC) /Fewc3270.exe $(CFLAGS) $(OBJECTS) $(LIBS)

..\lib\libw3270.lib:
	cd ..\lib\w3270 & $(MAKE) /nologo /fMsc\Makefile
..\lib\libw3270stubs.lib:
	cd ..\lib\w3270stubs & $(MAKE) /nologo /fMsc\Makefile

mkshort.exe: mkshort.obj shortcut.obj winvers.obj Msc\Makefile
	$(CC) /Femkshort.exe $(CFLAGS) \
		mkshort.obj shortcut.obj winvers.obj $(SHRTLIBS)

wversion.obj: version.txt mkversion.exe
	mkversion.exe -w
	$(CC) $(CFLAGS) /c wversion.c

wc3270wiz.exe: $(WOBJECTS)
	$(CC) /Fewc3270wiz.exe $(CFLAGS) $(WOBJECTS) $(WIZLIBS)

catf.exe: catf.c
	$(CC) /Fecatf.exe $(CFLAGS) catf.c

ead3270.exe: ead3270.obj windirs.obj
	$(CC) /Fe$@ $(CFLAGS) ead3270.obj windirs.obj advapi32.lib shell32.lib

ead3270.obj: ead3270.c
	$(CC) $(CFLAGS) /c ead3270.c

x3270if.exe: x3270if.obj ..\lib\lib3270.lib
	$(CC) /Fe$@ $(CFLAGS) x3270if.obj ..\lib\lib3270.lib ws2_32.lib

x3270if.obj: x3270if.c
	$(CC) $(CFLAGS) /c x3270if.c

clean:
	erase *.obj *.RES *.exp *.lib $(MKFB) $(MKKEYPAD) compiled_keypad.h fallbacks.c mkversion.exe version.c wversion.c

clobber: clean
	erase $(PROGS)

keypad.c: compiled_keypad.h
