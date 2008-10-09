This directory contains files needed to build wc3270 with command-line
Microsoft tools:

    Makefile
	Makefile for nmake.  To use this Makefile, cd to the directory above
	this one and run:
		nmake /f Msc\Makefile

    mkversion.c
	C-language replacement for mkversion.sh and mkwversion.sh.

    deprecated.h
	#defines to work around some MS-deprecated POSIX function names.

Official releases of wc3270 are built and tested with MinGW, not the Microsoft
tools.  These files are provided as-is, with no guarantee that the resulting
executables and DLLs will function properly.

Also note that the resulting executable will not include SSL support.
