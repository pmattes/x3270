# Copyright (c) 2016-2025 Paul Mattes.
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

# Makefile for Windows libexpat
xtop = ../..
top = ../../..
ifdef WIN64
objdir = $(xtop)/obj/win64/libexpat
else
objdir = $(xtop)/obj/win32/libexpat
endif
extern = $(xtop)/extern
srcdir = $(extern)/libexpat/expat
this = $(top)/lib/libexpat

export VPATH = $(this):$(top)/extern/libexpat/expat/lib
export TOP = $(top)
export THIS = $(this)

MAKEINC = -I$(top)/extern/libexpat/expat/lib -I$(top)/Common/Win32 -I$(this)

all: $(srcdir) $(objdir)
	$(MAKE) -C $(objdir) $(MAKEINC) -f $(this)/Makefile.obj $@
clean clobber: $(objdir)
	$(MAKE) -C $(objdir) $(MAKEINC) -f $(this)/Makefile.obj $@

$(objdir):
	mkdir -p $(objdir)

EXPAT_VERSION = R_2_4_8
tgz = $(EXPAT_VERSION).tar.gz
# If this is a git repo, init the libexpat submodule. Otherwise, get
# the libexpat tarball from github.
$(srcdir):
	if [ -d $(xtop)/.git ]; \
	then	git submodule update --init $(extern)/libexpat; \
	else	tmpdir=/tmp/expat$$$$; \
		mkdir $$tmpdir && \
		wget -P $$tmpdir https://github.com/libexpat/libexpat/archive/refs/tags/$(tgz) && \
		tar -C $$tmpdir --exclude=libexpat-$(EXPAT_VERSION)/README.md -xzf $$tmpdir/$(tgz) && \
		mv $$tmpdir/libexpat-$(EXPAT_VERSION)/* $(extern)/libexpat && \
		rm -rf $$tmpdir; \
	fi
