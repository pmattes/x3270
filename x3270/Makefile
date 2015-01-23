# Copyright (c) 2015 Paul Mattes.
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

# Top-level Makefile for suite3270.

# Unix targets.
UNIX_PRODS = c3270-dir s3270-dir tcl3270-dir x3270-dir pr3287-dir
UNIX_LIBS = lib-3270-dir lib-3270stubs-dir

# Windows targets.
WINDOWS_PRODS = wc3270-dir ws3270-dir wpr32787-dir
WINDOWS_LIBS = lib-w3270-dir lib-w3270stubs-dir

# Global targets.
PRODUCTS = $(UNIX_PRODS) $(WINDOWS_PRODS)

# The default top-level target is all of the products.
all: $(PRODUCTS) Webpage-dir

# Platform targets
unix: $(UNIX_PRODS)
windows: $(WINDOWS_PRODS)

# Library and shared dependencies.
$(UNIX_PRODS): unix-libs Shared-dir
$(WINDOWS_PRODS): windows-libs Shared-dir

# The webpage comes last.
Webpage-dir: $(PRODUCTS)

# Individual targets.

unix-libs:
	cd lib && $(MAKE) unix
windows-libs:
	cd lib && $(MAKE) windows

Shared-dir:
	cd Shared && $(MAKE)

include-dir:
	cd include && $(MAKE) -f Makefile.aux bootstrap

c3270-dir:
	cd c3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

s3270-dir:
	cd s3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

tcl3270-dir:
	cd tcl3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

x3270-dir:
	cd x3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

pr3287-dir:
	cd pr3287 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

wc3270-dir:
	cd wc3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

ws3270-dir:
	cd ws3270 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

wpr3287-dir:
	cd wpr3287 && $(MAKE) -f Makefile.aux bootstrap && $(MAKE)

Webpage-dir:
	cd Webpage && $(MAKE)

# Special pseudo-targets.
depend: unix-depend windows-depend
unix-depend: unix-bootstrap
	cd lib && $(MAKE) unix-depend
	cd c3270 && $(MAKE) depend
	cd s3270 && $(MAKE) depend
	cd tcl3270 && $(MAKE) depend
	cd x3270 && $(MAKE) depend
	cd pr3287 && $(MAKE) depend
windows-depend: windows-bootstrap
	cd lib && $(MAKE) windows-depend
	cd wc3270 && $(MAKE) depend
	cd ws3270 && $(MAKE) depend
	cd wpr3287 && $(MAKE) depend

clean: unix-clean windows-clean webpage-clean
unix-clean:
	cd lib && $(MAKE) unix-clean
	cd c3270 && $(MAKE) clean
	cd s3270 && $(MAKE) clean
	cd tcl3270 && $(MAKE) clean
	cd x3270 && $(MAKE) clean
	cd pr3287 && $(MAKE) clean
windows-clean:
	cd lib && $(MAKE) windows-clean
	cd wc3270 && $(MAKE) clean
	cd wpr3287 && $(MAKE) clean
	cd ws3270 && $(MAKE) clean
webpage-clean:
	cd Webpage && $(MAKE) clean

clobber: unix-clobber windows-clobber webpage-clobber
unix-clobber:
	cd lib && $(MAKE) unix-clobber
	cd c3270 && $(MAKE) clobber
	cd s3270 && $(MAKE) clobber
	cd tcl3270 && $(MAKE) clobber
	cd x3270 && $(MAKE) clobber
	cd pr3287 && $(MAKE) clobber
windows-clobber:
	cd lib && $(MAKE) windows-clobber
	cd wc3270 && $(MAKE) clobber
	cd wpr3287 && $(MAKE) clobber
	cd ws3270 && $(MAKE) clobber
webpage-clobber:
	cd Webpage && $(MAKE) clobber

bootstrap: unix-bootstrap windows-bootstrap include-bootstrap
unix-bootstrap:
	cd c3270 && $(MAKE) -f Makefile.aux bootstrap
	cd s3270 && $(MAKE) -f Makefile.aux bootstrap
	cd tcl3270 && $(MAKE) -f Makefile.aux bootstrap
	cd x3270 && $(MAKE) -f Makefile.aux bootstrap
	cd pr3287 && $(MAKE) -f Makefile.aux bootstrap
windows-bootstrap:
	cd wc3270 && $(MAKE) -f Makefile.aux bootstrap
	cd ws3270 && $(MAKE) -f Makefile.aux bootstrap
	cd wpr3287 && $(MAKE) -f Makefile.aux bootstrap
include-bootstrap:
	cd include && $(MAKE) -f Makefile.aux bootstrap
