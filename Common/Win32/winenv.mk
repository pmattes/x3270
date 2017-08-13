# Common definitions for Windows makefiles.

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
