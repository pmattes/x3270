# Common definitions for Windows makefiles.
#
# Figure out if they've installed MinGW OpenSSL.
SSLDIR = /usr/local/OpenSSL-Win32
HAVE_OPENSSL = $(shell [ -d $(SSLDIR) ] && echo yes)
ifeq ($(HAVE_OPENSSL),yes)
ifndef NO_SSL
SSLCPP = -DHAVE_LIBSSL=1 -I$(SSLDIR)/include
SSLLIB = -lwsock32 -ladvapi32 -lgdi32 -luser32
endif
endif

# Set the MinGW tools prefix based on the WIN64 environment variable.
# This is a little clumsy for now, but at least it's possible.
ifdef WIN64
GT_PFX = x86_64-w64-mingw32-
else
GT_PFX = i686-w64-mingw32-
endif

NATIVECC = gcc
CC = $(GT_PFX)gcc
AR = $(GT_PFX)ar
WINDRES = $(GT_PFX)windres

# Set the local executable suffix, depending on whether we are compiling on
# Cygwin or Linux.
OS = $(shell uname -o)
ifeq ($(OS),Cygwin)
NATIVE_SFX = .exe
else
NATIVE_SFX =
endif

# Srt up the common Windows compiler flags
WIN32_FLAGS = -D_WIN32
