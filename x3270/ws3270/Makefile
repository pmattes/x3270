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

# Makefile for ws3270

# Set SSLDIR, SSLCPP, SSLLIB, GT_PFX, NATIVE_SFX, NATIVECC, CC, WINDRES and
#  WIN32_FLAGS.
include winenv.mk

MKFB = mkfb$(NATIVE_SFX)

XCPPFLAGS = $(WIN32_FLAGS) -DWIN32_LEAN_AND_MEAN -I. -I../include
CFLAGS = $(EXTRA_FLAGS) -g -Wall -Werror $(XCPPFLAGS) $(SSLCPP)

OBJ = o
include common_files.mk ws3270_files.mk

SRCS = $(COMMON_SOURCES) $(HTTPD_SOURCES) $(WS3270_SOURCES)

VOBJS = $(COMMON_OBJECTS) $(HTTPD_OBJECTS) $(WS3270_OBJECTS) fallbacks.o \
	ws3270res.o

OBJECTS = $(VOBJS) version.o
LIBS = $(SSLLIB) -lws2_32 -lcomdlg32 -lgdi32 -lwinspool
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

ws3270.exe: $(OBJECTS) Makefile ../lib/libw3270.a ../lib/libw3270stubs.a
	$(CC) -o ws3270.exe $(CFLAGS) $(OBJECTS) $(LIBS) -L../lib -lw3270 -lw3270stubs

../lib/libw3270.a:
	cd ../lib/w3270 && $(MAKE) all
../lib/libw3270stubs.a:
	cd ../lib/w3270stubs && $(MAKE) all

wversion.o: version.txt mkwversion.sh
	@chmod +x mkwversion.sh version.txt
	sh ./mkwversion.sh $(CC)

clean:
	rm -f *.o mkfb mkfb.exe fallbacks.c dependencies-

clobber: clean
	rm -f $(PROGS)

depend: $(SRCS)
	mingwmakedep -C $(CC) $(XCPPFLAGS) $(SRCS) >dependencies-
	mv dependencies- dependencies

ifeq ($(wildcard dependencies),dependencies)
include dependencies
endif

