# Copyright (c) 2007-2009, 2013-2015 Paul Mattes.
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

# Set SSLDIR, SSLCPP, SSLLIB, GT_PFX, NATIVE_SFX, NATIVECC, CC, WINDRES and
#  WIN32_FLAGS.
include winenv.mk

MKFB = mkfb$(NATIVE_SFX)
MKKEYPAD = mkkeypad$(NATIVE_SFX)

XCPPFLAGS = $(WIN32_FLAGS) -I. -I../include
CFLAGS = $(EXTRA_FLAGS) -g -Wall -Werror $(XCPPFLAGS) $(SSLCPP)

OBJ = o
include common_files.mk wc3270_files.mk

SRCS = $(COMMON_SOURCES) $(WC3270_SOURCES)

VOBJS = $(COMMON_OBJECTS) $(WC3270_OBJECTS) fallbacks.o wc3270res.o

LIB3270 = ../lib/libw3270.a
LIB3270I = ../lib/libw3270i.a
LIBSTUBS = ../lib/libw3270stubs.a

OBJECTS = $(VOBJS) version.o
WOBJECTS = wizard.o wc3270res.o wversion.o shortcut.o relink.o
LIBS = $(SSLLIB) -lws2_32 -lole32 -luuid -lcomdlg32 -lgdi32 -lwinspool
SHRTLIBS = -L../lib -lw3270 -lole32 -luuid
WIZLIBS = -L../lib -lw3270 -lole32 -luuid -lwinspool
DLLFLAGS = $(EXTRA_FLAGS) -mno-cygwin -shared -Wl,--export-all-symbols -Wl,--enable-auto-import

PROGS = wc3270.exe mkshort.exe wc3270wiz.exe catf.exe ead3270.exe x3270if.exe
all: $(PROGS)

wc3270.exe : XCPPFLAGS += -DWIN32_LEAN_AND_MEAN

version.o: $(VOBJS) version.txt mkversion.sh Makefile
	$(RM) xversion.c
	@chmod +x mkversion.sh version.txt
	sh ./mkversion.sh wc3270 >xversion.c
	$(CC) $(CFLAGS) -c -o $@ xversion.c
	$(RM) xversion.c

fallbacks.c: $(MKFB) X3270.xad Makefile
	$(RM) $@
	./$(MKFB) -c X3270.xad $@

$(MKFB): mkfb.c Makefile
	$(NATIVECC) -g -o $(MKFB) -DFOR_WIN32 mkfb.c

keypad.o: keypad.c compiled_keypad.h

compiled_keypad.h: keypad.full keypad.labels keypad.map keypad.outline keypad.callbacks $(MKKEYPAD)
	./$(MKKEYPAD) >$@

$(MKKEYPAD): mkkeypad.c
	$(NATIVECC) -g -o $(MKKEYPAD) mkkeypad.c

wc3270res.o: wc3270.rc wc3270.ico Makefile
	$(WINDRES) -i wc3270.rc -o wc3270res.o

wc3270.exe: $(OBJECTS) Makefile $(LIB3270) $(LIB3270I) $(LIBSTUBS)
	$(CC) -o wc3270.exe $(CFLAGS) $(OBJECTS) -L../lib -lw3270 -lw3270i -lw3270stubs $(LIBS)

$(LIB3270) $(LIB3270I):
	cd ../lib/w3270 && $(MAKE) all
$(LIBSTUBS):
	cd ../lib/w3270stubs && $(MAKE) all

mkshort.exe: mkshort.o shortcut.o $(LIB3270) Makefile
	$(CC) -o mkshort.exe $(CFLAGS) mkshort.o shortcut.o $(SHRTLIBS)

wversion.o: version.txt mkwversion.sh
	@chmod +x mkwversion.sh version.txt
	sh ./mkwversion.sh $(CC)

wc3270wiz.exe: $(WOBJECTS) $(LIB3270)
	$(CC) -o wc3270wiz.exe $(CFLAGS) $(WOBJECTS) $(WIZLIBS)

catf.exe: catf.c
	$(CC) -o catf.exe $(CFLAGS) catf.c

ead3270.exe: ead3270.o $(LIB3270)
	$(CC) -o $@ $(CFLAGS) ead3270.o -L../lib -lw3270

ead3270.o: ead3270.c
	$(CC) $(CFLAGS) -c ead3270.c

x3270if.exe: x3270if.o $(LIB3270)
	$(CC) -o $@ $(CFLAGS) x3270if.o -L../lib -lw3270 -lws2_32

clean:
	rm -f *.o $(MKFB) $(MKKEYPAD) compiled_keypad.h fallbacks.c dependencies-

clobber: clean
	rm -f $(PROGS)

depend: $(SRCS) compiled_keypad.h
	$(CC) -MM $(XCPPFLAGS) $(SRCS) >dependencies-
	mv dependencies- dependencies

ifeq ($(wildcard dependencies),dependencies)
include dependencies
endif
