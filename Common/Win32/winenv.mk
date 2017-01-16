# Common definitions for Windows makefiles.
#
# Figure out if they've installed MinGW OpenSSL.
SSLDIR = /usr/local/OpenSSL-Win32-1_1_0c
HAVE_OPENSSL = $(shell [ -d $(SSLDIR) ] && echo yes)
ifeq ($(HAVE_OPENSSL),yes)
ifndef NO_SSL
SSLCPP = -DHAVE_LIBSSL=1 -I$(SSLDIR)/include
SSLLIB = -lwsock32 -ladvapi32 -lgdi32 -luser32
endif
endif

# Set the MinGW tools prefix and the _WIN32/64 symbols based on the WIN64
# environment variable.
ifdef WIN64
GT_PFX = x86_64-w64-mingw32-
WIN32_FLAGS = -D_WIN32 -D_WIN64
else
GT_PFX = i686-w64-mingw32-
WIN32_FLAGS = -D_WIN32
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
