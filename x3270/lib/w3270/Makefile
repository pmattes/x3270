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

# Makefile for 3270 emulation library (Windows)

LIB3270 = ../libw3270.a
LIB3270I = ../libw3270i.a
LIB32XX = ../libw32xx.a

include winenv.mk

XCPPFLAGS = $(WIN32_FLAGS) -I. -I../../include
CFLAGS = $(EXTRA_FLAGS) -g -Wall -Werror $(XCPPFLAGS) $(SSLCPP)

all:: $(LIB3270) $(LIB3270I) $(LIB32XX)

OBJ = o
include lib3270_files.mk
include libw3270_files.mk
include lib3270i_files.mk
include lib32xx_files.mk
include libw32xx_files.mk
SRCS = $(LIB3270_SOURCES) $(LIBW3270_SOURCES)
OBJS = $(LIB3270_OBJECTS) $(LIBW3270_OBJECTS)

$(LIB3270): $(OBJS)
	$(AR) crs $@ $(OBJS)

$(LIB3270I): $(LIB3270I_OBJECTS)
	$(AR) crs $@ $(LIB3270I_OBJECTS)

$(LIB32XX): $(LIB32XX_OBJECTS) $(LIBW32XX_OBJECTS)
	$(AR) crs $@ $(LIB32XX_OBJECTS) $(LIBW32XX_OBJECTS)

favicon.o: favicon.c

favicon.c: favicon.ico mkicon$(NATIVE_SFX)
	./mkicon$(NATIVE_SFX) favicon.ico >favicon.c

mkicon$(NATIVE_SFX): mkicon.c
	$(NATIVECC) $(CFLAGS) -o $@ mkicon.c

clean:
	$(RM) *.o favicon.c mkicon$(NATIVE_SFX)

clobber: clean
	$(RM) $(LIB3270)

depend: $(SRCS) $(LIB3270I_SOURCES) Makefile lib3270_files.mk lib3270i_files.mk libw3270_files.mk
	$(CC) -MM $(XCPPFLAGS) -I. $(SRCS) $(LIB3270I_SOURCES) $(LIB32XX_SOURCES) >dependencies-
	mv dependencies- dependencies

ifeq ($(wildcard dependencies),dependencies)
include dependencies
endif
