# Common definitions for Windows makefiles.

# Set the MinGW tools prefix and the _WIN32/64 symbols based on the WIN64
# environment variable.
# Also set HOST (used to build libraries),
ifdef WIN64
GT_PFX = x86_64-w64-mingw32-
WIN32_FLAGS = -D_WIN32 -D_WIN64
HOST = win64
else
GT_PFX = i686-w64-mingw32-
WIN32_FLAGS = -D_WIN32
HOST = win32
endif

CC = $(GT_PFX)gcc
AR = $(GT_PFX)gcc-ar
ifndef WINDRES
WINDRES = $(GT_PFX)windres
endif

# Set the build host's executable suffix, depending on whether we are
# compiling on Cygwin, Msys or Linux.
BUILDXSFX =
OS = $(shell uname -o)
ifeq ($(OS),Cygwin)
BUILDXSFX = .exe
endif
ifeq ($(OS),Msys)
BUILDXSFX = .exe
endif
