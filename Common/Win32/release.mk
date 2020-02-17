# Copyright (c) 1995-2016, 2018-2020 Paul Mattes.
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

# Makefile definitions for code release.

# Set the version.
VERSION = $(shell . ./version.txt && echo $$version)

# Find Inno Setup and kSign.
# On 32-bit Wine, they are in C:\Program Files.
# On 64-bit Wine, they are in C:\Program Files (x86).
ifeq ($(shell test -d "`winepath -u \"C:\\Program Files (x86)\"`" && echo yes),yes)
PFX = C:\Program Files (x86)
QPFX = C:\\Program Files (x86)
QQPFX = C:\\Program\ Files\ \(x86\)
else
PFX = C:\Program Files
QPFX = C:\\Program Files
QQPFX = C:\\Program\ Files
endif

# Figure out the files to sign and the files to build.
SIGNFILES32 = $(shell awk '/\.exe/ { print $$2 }' *-32.zipit)
SIGNFILES64 = $(shell awk '/\.exe/ { print $$2 }' *-64.zipit)
SIGNFILES = $(SIGNFILES32) $(SIGNFILES64)
BUILDFILES = $(shell awk '/\.exe/ { print $$3 }' *-64.zipit | sed -e 's/.exe//' -e 's/^[^w]/w&/')

# Set the certificate file path.
CERT = z:\\hd\xfer\Cert2020.p12

# How to sign files.
sign-files:
	@../Common/Win32/readpass
	@(/bin/echo '@echo off'; \
	 for i in $(SIGNFILES); \
	    do /bin/echo -E "\"$(QPFX)\\kSign\\ksigncmd.exe\" /f $(CERT) /p \"`sed 's/%/%%/' /tmp/pass`\" $$i"; \
	       /bin/echo "if errorlevel 1 ("; \
	       /bin/echo "    goto :EOF"; \
	       /bin/echo ")"; \
	       /bin/echo -E "echo ==== $$i ===="; \
	    done) >/tmp/sign.bat
	wine cmd /c Z:\\tmp\\sign.bat
	@$(RM) /tmp/sign.bat

# How to build .exe files.
BUILDOPTS = -j4 -Werror -Wno-format-zero-length
buildexe:
	cd .. && $(MAKE) $(BUILDOPTS) $(BUILDFILES)

# Set up build dependencies.
ZIPITDEP32 = $(SIGNFILES32)
ZIPITDEP64 = $(SIGNFILES64)

ZIPIT = ../Common/Win32/zipit
